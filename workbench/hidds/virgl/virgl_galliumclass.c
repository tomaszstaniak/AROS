/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <aros/debug.h>
#include <proto/oop.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/dos.h>
#include <dos/var.h>
#include <exec/tasks.h>
#include <hidd/gallium.h>
#include <gallium/gallium.h>
#include "pipe/p_screen.h"
#include "virgl_stats.h"

#include "virgl/virgl_resource.h"
#include "virgl_winsys_aros.h"
#include "virgl_intern.h"
#define CyberGfxBase (SD(cl)->CyberGfxBase)
#define UtilityBase (SD(cl)->UtilityBase)
#undef HiddGalliumAttrBase
#define HiddGalliumAttrBase (SD(cl)->hiddGalliumAB)
/* ENV:VirGL-Status tells the user what the renderer is doing: "virgl", or
 * "virgl: ..." when the host is slow or has stopped answering. Only written
 * from a DOS process. */
void avgl_report(const char *status)
{
    struct Task *me=FindTask(NULL);
    struct Library *DOSBase;
    if(me->tc_Node.ln_Type!=NT_PROCESS)return;
    DOSBase=OpenLibrary("dos.library",36);
    if(!DOSBase)return;
    SetVar("VirGL-Status",status,-1,GVF_GLOBAL_ONLY|LV_VAR);
    CloseLibrary(DOSBase);
}
/* "softpipe: <why>". No sprintf: this module is resident (see gallium.library). */
static void report_softpipe(const char *why)
{
    static const char head[]="softpipe: ";
    char text[128];
    ULONG n=0;
    for(const char *c=head;*c;c++)text[n++]=*c;
    for(const char *c=why;*c && n<sizeof(text)-1;c++)text[n++]=*c;
    text[n]=0;
    avgl_report(text);
}
/* The winsys and the screen for this object; call with screen_lock held. */
static struct pipe_screen *create_screen(OOP_Class *cl,struct HiddGalliumVirglData *d)
{
    if(!d->winsys && !d->screen && d->factory.module){
        d->winsys=avgl_winsys_create(&d->winsys,&d->factory,&d->manager,&SD(cl)->registry);
        if(d->winsys){
            d->screen=d->factory.create(d->factory.module,GALLIUM_SCREEN_BACKEND_VIRGL,d->winsys);
            /* A failed core constructor may already consume the winsys;
             * its native destroy clears this owner slot. */
            if(!d->screen && d->winsys)avgl_winsys_dispose(d->winsys);
            if(!d->screen)d->manager.error="Mesa could not set up a VirGL screen";
            else{avgl_activity.gave_up=FALSE;avgl_report("virgl");}
        }
    }
    return d->screen;
}
OOP_Object *METHOD(HiddVirgl,Root,New)
{
    if(GetTagData(aHidd_Gallium_InterfaceVersion,-1,msg->attrList)!=GALLIUM_INTERFACE_VERSION)return NULL;
    o=(OOP_Object*)OOP_DoSuperMethod(cl,o,(OOP_Msg)msg);
    if(o){
        struct HiddGalliumVirglData *d=OOP_INST_DATA(cl,o);
        const struct GalliumScreenFactory *factory=(const void *)
            GetTagData(aHidd_Gallium_ScreenFactory,0,msg->attrList);
        d->winsys=NULL;d->screen=NULL;d->factory.module=NULL;
        avgl_manager_init(&d->manager,UtilityBase);
        InitSemaphore(&d->screen_lock);
        if(factory){
            struct Library *module=NULL;
            if(factory->version==GALLIUM_SCREEN_FACTORY_VERSION &&
               factory->size>=sizeof(*factory) &&
               (factory->backends&GALLIUM_SCREEN_BACKEND_VIRGL) &&
               factory->module && factory->create && factory->destroy && factory->format)
                module=OpenLibrary(factory->module->lib_Node.ln_Name,factory->module->lib_Version);
            if(!module){OOP_DisposeObject(o);return NULL;}
            d->factory=*factory;d->factory.module=module;
        }
        OOP_Object *service=(APTR)GetTagData(aHidd_Gallium_Transport,0,msg->attrList);
        struct pHidd_VirtioGPU_Acquire acquire={OOP_GetMethodID(IID_Hidd_VirtioGPU,moHidd_VirtioGPU_Acquire),VIRTIOGPU_TRANSPORT_VERSION};
        if(!d->factory.module || !service || !OOP_DoMethod(service,(OOP_Msg)&acquire)){
            OOP_DisposeObject(o);return NULL;
        }
        d->manager.service=service;
        /* Make the screen now: a pipe that cannot get one would leave the
         * program without OpenGL, while a refusal here lets gallium.library
         * fall back to softpipe. */
        ObtainSemaphore(&d->screen_lock);
        struct pipe_screen *screen=create_screen(cl,d);
        ReleaseSemaphore(&d->screen_lock);
        if(!screen){
            report_softpipe(d->manager.error?d->manager.error:"VirGL not usable");
            OOP_DisposeObject(o);return NULL;
        }
    }
    return o;
}
VOID METHOD(HiddVirgl,Root,Dispose)
{
    struct HiddGalliumVirglData *d=OOP_INST_DATA(cl,o);
    if(d->screen)d->factory.destroy(d->factory.module,d->screen);
    d->screen=NULL;
    if(d->winsys)avgl_winsys_dispose(d->winsys);
    if(d->manager.service){
        struct pHidd_VirtioGPU_Release release={OOP_GetMethodID(IID_Hidd_VirtioGPU,moHidd_VirtioGPU_Release)};
        OOP_DoMethod(d->manager.service,(OOP_Msg)&release);
    }
    if(d->factory.module){
        struct Library *module=d->factory.module;d->factory.module=NULL;CloseLibrary(module);
    }
    OOP_DoSuperMethod(cl,o,(OOP_Msg)msg);
}
VOID METHOD(HiddVirgl,Root,Get)
{
    ULONG idx;
    if(IS_GALLIUM_ATTR(msg->attrID,idx) && idx==aoHidd_Gallium_InterfaceVersion){*msg->storage=GALLIUM_INTERFACE_VERSION;return;}
    OOP_DoSuperMethod(cl,o,(OOP_Msg)msg);
}
APTR METHOD(HiddVirgl,Hidd_Gallium,CreatePipeScreen)
{
    struct HiddGalliumVirglData *d=OOP_INST_DATA(cl,o);
    struct pipe_screen *screen=NULL;
    ObtainSemaphore(&d->screen_lock);
    create_screen(cl,d);
    screen=d->screen;
    ReleaseSemaphore(&d->screen_lock);
    return screen;
}
VOID METHOD(HiddVirgl,Hidd_Gallium,DestroyPipeScreen)
{
    struct HiddGalliumVirglData *d=OOP_INST_DATA(cl,o);
    ObtainSemaphore(&d->screen_lock);
    if(d->screen && msg->screen==d->screen){
        d->factory.destroy(d->factory.module,d->screen);d->screen=NULL;
    }
    ReleaseSemaphore(&d->screen_lock);
}
VOID METHOD(HiddVirgl,Hidd_Gallium,DisplayResource)
{
    struct HiddGalliumVirglData *d=OOP_INST_DATA(cl,o);
    struct virgl_resource *res=virgl_resource(msg->resource);
    struct RastPort *rp;
    APTR pixels;
    ULONG format;
    if(!d->winsys || !res || !res->hw_res)return;
    switch(res->b.format){
      case PIPE_FORMAT_R8G8B8A8_UNORM:case PIPE_FORMAT_R8G8B8A8_SRGB:format=RECTFMT_RGBA;break;
      case PIPE_FORMAT_B8G8R8A8_UNORM:case PIPE_FORMAT_B8G8R8X8_UNORM:
      case PIPE_FORMAT_B8G8R8A8_SRGB:case PIPE_FORMAT_B8G8R8X8_SRGB:format=RECTFMT_BGRA32;break;
      default:bug("[virgl.hidd] unsupported presentation format %u\n",res->b.format);return;
    }
    UQUAD t0=avgl_stat_now(),t1;
    if(!avgl_read_resource(d->winsys,res->hw_res,res->b.width0,res->b.height0,res->metadata.stride[0]))return;
    avgl_stat_add(AVGL_ST_PRESENT_READ,t0,res->metadata.stride[0]*res->b.height0);
    pixels=d->winsys->resource_map(d->winsys,res->hw_res);
    if(!pixels)return;
    rp=CreateRastPort();if(!rp)return;
    rp->BitMap=msg->bitmap;
    t1=avgl_stat_now();
    WritePixelArray(pixels,msg->srcx,msg->srcy,
        res->metadata.stride[0],rp,msg->dstx,msg->dsty,msg->width,msg->height,format);
    avgl_stat_add(AVGL_ST_PRESENT_COPY,t1,msg->width*msg->height*4);
    FreeRastPort(rp);
    avgl_stat_add(AVGL_ST_PRESENT,t0,0);
}
