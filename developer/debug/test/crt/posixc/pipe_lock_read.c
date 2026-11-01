/* Exercise the third PIPEKEY constructor: OpenFromLock must start blocking. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <dos/dosextens.h>
#include <dos/pipe.h>
#include <proto/dos.h>
static BPTR writer;
static atomic_int written;
static long now_ms(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000+t.tv_nsec/1000000;}
static void *produce(void *unused) {(void)unused;Write(writer,(APTR)"X",1);atomic_store(&written,1);usleep(300000);Close(writer);writer=0;return NULL;}
int main(void) {
 char name[80],data[64];int rc=2,started=0;BPTR seed=0,lock=0,reader=0;pthread_t t;
 snprintf(name,sizeof(name),"PIPE:hf-lock-%ld",(long)getpid());
 writer=Open((CONST_STRPTR)name,MODE_NEWFILE);if(!writer)goto out;
 /* Leave a nonblocking key in the allocator's free list before conversion. */
 seed=Open((CONST_STRPTR)name,MODE_OLDFILE);rc=3;if(!seed)goto out;
 struct FileHandle *h=BADDR(seed);
 rc=77;if(DoPkt(h->fh_Type,ACTION_PIPE_SET_NONBLOCK,h->fh_Arg1,1,0,0,0)!=1)goto out;
 Close(seed);seed=0;
 lock=Lock((CONST_STRPTR)name,SHARED_LOCK);rc=4;if(!lock)goto out;
 reader=OpenFromLock(lock);rc=5;if(!reader)goto out;lock=0;
 rc=6;if(pthread_create(&t,NULL,produce,NULL))goto out;started=1;
 while(!atomic_load(&written))sched_yield();
 long start=now_ms();LONG n=Read(reader,data,sizeof(data));long elapsed=now_ms()-start;
 pthread_join(t,NULL);started=0;
 printf("PIPE_LOCK_READ n=%ld byte=%d elapsed_ms=%ld\n",(long)n,n>0?data[0]:0,elapsed);
 rc=n==1&&data[0]=='X'&&elapsed>=200?0:20;
out:
 if(started)pthread_join(t,NULL);
 if(reader)Close(reader);
 if(seed)Close(seed);
 if(lock)UnLock(lock);
 if(writer)Close(writer);
 return rc;
}
