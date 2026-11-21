/* Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Protocol client: PCI, queues, DMA and reset belong to virtiogfx. */
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/oop.h>
#include <aros/debug.h>
#include "virgl_transport.h"
#include "virgl_native.h"
#include "virgl_stats.h"
static OOP_MethodID method(ULONG i){return OOP_GetMethodID(IID_Hidd_VirtioGPU,i);}
struct avgl_activity avgl_activity;
void avgl_manager_init(struct avgl_manager *m,struct Library *u)
{
    InitSemaphore(&m->mutex);
    m->utility_base=u;
    m->references=0;
    m->service=NULL;
    m->error=NULL;
    m->initialized=TRUE;
}
BOOL avgl_manager_can_expunge(struct avgl_manager *m)
{
    return !m->initialized || !m->references;
}
BOOL avgl_healthy(struct avgl_transport *s)
{
    struct pHidd_VirtioGPU_Healthy msg={method(moHidd_VirtioGPU_Healthy)};
    return s->service && !s->failed && OOP_DoMethod(s->service,(OOP_Msg)&msg);
}
static BOOL command(struct avgl_transport *s,const void *p,ULONG n,ULONG type,ULONG size,void *out)
{
    struct pHidd_VirtioGPU_Command msg={method(moHidd_VirtioGPU_Command),p,n,out,size,type};
    UQUAD t=avgl_stat_now();
    BOOL ok;
    if(!s->service)return FALSE;
    __atomic_fetch_add(&avgl_activity.running,1,__ATOMIC_RELAXED);
    ok=OOP_DoMethod(s->service,(OOP_Msg)&msg);
    __atomic_fetch_add(&avgl_activity.finished,1,__ATOMIC_RELAXED);
    __atomic_fetch_sub(&avgl_activity.running,1,__ATOMIC_RELAXED);
    /* The watcher said that the host is slow; this is the answer. */
    if(ok && avgl_activity.told){avgl_activity.told=FALSE;avgl_report("virgl");}
    avgl_stat_add(AVGL_ST_COMMAND,t,n);
    return ok;
}
BOOL avgl_busy(struct avgl_transport *s)
{
    struct pHidd_VirtioGPU_Healthy msg={method(moHidd_VirtioGPU_Healthy)};
    return s->service && !OOP_DoMethod(s->service,(OOP_Msg)&msg);
}
BOOL avgl_command(struct avgl_transport *s,const void *p,ULONG n,ULONG type,ULONG min,ULONG *got,BOOL fenced)
{
    struct gpu_header reply;
    (void)fenced;
    if(!got || min!=sizeof(reply) || type!=GPU_OK_NODATA || s->failed)return FALSE;
    *got=0;
    BOOL ok=command(s,p,n,type,sizeof(reply),&reply);
    if(ok){
        *got=sizeof(reply);
        if(((const struct gpu_header*)p)->type==GPU_RESOURCE_CREATE_3D)s->resources_live++;
    }else if(avgl_busy(s)){
        /* Not a refusal: the host did not answer within the limit. */
        avgl_activity.gave_up=TRUE;
        avgl_report("virgl: stopped, the host did not answer");
    }
    return ok;
}
BOOL avgl_cleanup_command(struct avgl_transport *s,const void *p,ULONG n)
{
    struct gpu_header reply;
    BOOL ok=command(s,p,n,GPU_OK_NODATA,sizeof(reply),&reply);
    if(ok && ((const struct gpu_header*)p)->type==GPU_RESOURCE_UNREF && s->resources_live)s->resources_live--;
    return ok;
}
ULONG avgl_resource_id(struct avgl_transport *s)
{
    struct pHidd_VirtioGPU_AllocateID msg={method(moHidd_VirtioGPU_AllocateID),FALSE};
    return avgl_healthy(s)?OOP_DoMethod(s->service,(OOP_Msg)&msg):0;
}
APTR avgl_alloc(struct avgl_transport *s,ULONG n)
{
    struct pHidd_VirtioGPU_Allocate msg={method(moHidd_VirtioGPU_Allocate),n};
    return avgl_healthy(s)?(APTR)OOP_DoMethod(s->service,(OOP_Msg)&msg):NULL;
}
void avgl_free(struct avgl_transport *s,APTR p)
{
    struct pHidd_VirtioGPU_Free msg={method(moHidd_VirtioGPU_Free),p};
    if(s->service)OOP_DoMethod(s->service,(OOP_Msg)&msg);
}
UQUAD avgl_dma_address(struct avgl_transport *s,APTR p)
{
    UQUAD address=~(UQUAD)0;
    struct pHidd_VirtioGPU_Address msg={method(moHidd_VirtioGPU_Address),p,&address};
    if(s->service)OOP_DoMethod(s->service,(OOP_Msg)&msg);
    return address;
}
BOOL avgl_backing_begin(struct avgl_transport *s,APTR p,ULONG flags)
{
    struct pHidd_VirtioGPU_BackingBegin msg={method(moHidd_VirtioGPU_BackingBegin),p,flags};
    return s->service && OOP_DoMethod(s->service,(OOP_Msg)&msg);
}
void avgl_backing_end(struct avgl_transport *s,APTR p)
{
    struct pHidd_VirtioGPU_BackingEnd msg={method(moHidd_VirtioGPU_BackingEnd),p};
    if(s->service)OOP_DoMethod(s->service,(OOP_Msg)&msg);
}
/* The manager lives in the driver object. A transport that outlives the
 * object, to be closed later by the cleanup process, must leave it first. */
void avgl_leave_manager(struct avgl_transport *s)
{
    struct avgl_manager *m=s->manager;
    if(!m)return;
    s->manager=NULL;
    ObtainSemaphore(&m->mutex);
    m->references--;
    ReleaseSemaphore(&m->mutex);
}
BOOL avgl_close(struct avgl_transport *s)
{
    if(!s->service)return TRUE;
    if(s->context_created){
        struct gpu_header req={.type=GPU_CTX_DESTROY,.context=s->context},reply;
        if(!command(s,&req,sizeof(req),GPU_OK_NODATA,sizeof(reply),&reply))s->failed=TRUE;
    }
    /* Resources that could not be cleaned up stay with the owner's identity
     * until physical stop; say so once per client that leaves them behind. */
    if(s->failed || s->resources_live)
        bug("[virgl] client close ctx=%lu resources_left=%lu failed=%lu\n",
            (unsigned long)s->context,(unsigned long)s->resources_live,(unsigned long)s->failed);
    struct pHidd_VirtioGPU_Release msg={method(moHidd_VirtioGPU_Release)};
    OOP_DoMethod(s->service,(OOP_Msg)&msg);
    s->service=NULL;
    s->context_created=FALSE;
    avgl_leave_manager(s);
    return !s->failed;
}
BOOL avgl_open(struct avgl_transport *s,struct avgl_manager *m)
{
    struct pHidd_VirtioGPU_Acquire acquire={method(moHidd_VirtioGPU_Acquire),VIRTIOGPU_TRANSPORT_VERSION};
    struct pHidd_VirtioGPU_AllocateID id={method(moHidd_VirtioGPU_AllocateID),TRUE};
    struct render_context req={.header.type=GPU_CTX_CREATE,.length=17,.name="AROS shared VirGL"};
    struct gpu_header reply;
    avgl_native_clear(m->utility_base,s,sizeof(*s));
    s->manager=m;
    s->utility_base=m->utility_base;
    m->error="host is busy with an earlier command";
    if(!m->service || !OOP_DoMethod(m->service,(OOP_Msg)&acquire))return FALSE;
    m->error="host did not answer a capability query";
    s->service=m->service;
    ObtainSemaphore(&m->mutex);
    m->references++;
    ReleaseSemaphore(&m->mutex);
    /* Find the VirGL capset (ID 1) among the host's capsets and fetch it. */
    for(ULONG i=0;i<16;i++){
        struct capset_request query={.header.type=GPU_GET_CAPSET_INFO,.id=i};
        struct capset_info info;
        if(!command(s,&query,sizeof(query),GPU_OK_CAPSET_INFO,sizeof(info),&info))goto fail;
        if(info.id!=GPU_CAPSET_VIRGL)continue;
        if(!info.size || info.size>sizeof(s->caps)-sizeof(struct gpu_header)){
            m->error="host offers a VirGL capability set this driver cannot read";goto fail;
        }
        UBYTE *data=AllocVec(sizeof(struct gpu_header)+info.size,MEMF_PUBLIC);
        if(!data){m->error="not enough memory";goto fail;}
        query.header.type=GPU_GET_CAPSET;
        query.id=GPU_CAPSET_VIRGL;
        query.version=info.version;
        BOOL ok=command(s,&query,sizeof(query),GPU_OK_CAPSET,sizeof(struct gpu_header)+info.size,data);
        if(ok){
            CopyMem(data+sizeof(struct gpu_header),s->caps,info.size);
            s->caps_size=info.size;
        }
        FreeVec(data);
        if(!ok)goto fail;
        break;
    }
    if(!s->caps_size){m->error="host offers no VirGL capability set";goto fail;}
    if(!(s->context=OOP_DoMethod(s->service,(OOP_Msg)&id))){m->error="no free VirGL context number";goto fail;}
    req.header.context=s->context;
    if(!command(s,&req,sizeof(req),GPU_OK_NODATA,sizeof(reply),&reply)){
        m->error="host refused a VirGL context";goto fail;
    }
    s->context_created=TRUE;
    m->error=NULL;
    return TRUE;
fail:
    if(avgl_busy(s))m->error="host is busy with an earlier command";
    avgl_close(s);
    return FALSE;
}
