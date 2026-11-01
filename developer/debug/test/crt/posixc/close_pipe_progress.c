/* Finite test: close of a dup must not block a writer behind the fd table. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <proto/dos.h>
extern int __get_default_file(int,long *);
static int p[2], copy;
static BPTR raw_writer;
static atomic_int reading, closing;
static long close_ms;
static int close_rc, write_rc, read_rc;
static long now_ms(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000+t.tv_nsec/1000000;}
static void *reader(void *unused) {char b;(void)unused;atomic_store(&reading,1);read_rc=read(p[0],&b,1);return NULL;}
static void *closer(void *unused) {(void)unused;long start=now_ms();atomic_store(&closing,1);close_rc=close(copy);close_ms=now_ms()-start;return NULL;}
static void *writer(void *unused) {(void)unused;write_rc=write(p[1],"W",1);return NULL;}
static void *rescue(void *unused) {(void)unused;usleep(600000);Write(raw_writer,(APTR)"R",1);return NULL;}
int main(void) {
    pthread_t r,c,w,t;long handle=0;
    if(pipe(p) || (copy=dup(p[0]))<0 || __get_default_file(p[1],&handle))return 2;
    raw_writer=(BPTR)handle;
    if(pthread_create(&r,NULL,reader,NULL))return 3;
    while(!atomic_load(&reading))sched_yield();
    usleep(100000);
    if(pthread_create(&t,NULL,rescue,NULL) || pthread_create(&c,NULL,closer,NULL))return 3;
    while(!atomic_load(&closing))sched_yield();
    usleep(50000);
    if(pthread_create(&w,NULL,writer,NULL))return 3;
    pthread_join(c,NULL);pthread_join(w,NULL);pthread_join(r,NULL);pthread_join(t,NULL);
    close(p[0]);close(p[1]);
    printf("CLOSE_PIPE_PROGRESS close_rc=%d close_ms=%ld read=%d write=%d\n",close_rc,close_ms,read_rc,write_rc);
    return close_rc==0 && close_ms<300 && read_rc==1 && write_rc==1?0:20;
}
