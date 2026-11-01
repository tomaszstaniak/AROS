/* Finite descriptor-table snapshot versus sibling close/open regression. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
static atomic_int ready, stopped, errors;
static int channel[2];
static void *churn(void *unused)
{
    (void)unused;
    atomic_store(&ready,1);
    for (int i=0;i<2000 && !atomic_load(&stopped);i++) {
        int fd=open("NIL:",O_WRONLY);
        if(fd<0 || close(fd))atomic_fetch_add(&errors,1);
        DIR *dir=opendir("RAM:");
        if(!dir || closedir(dir))atomic_fetch_add(&errors,1);
    }
    return NULL;
}
int main(void)
{
    int held[32], status;
    volatile int failures=0;
    pthread_t thread;
    for(int i=0;i<32;i++) {
        held[i]=open("NIL:",O_WRONLY);
        if(held[i]<0)return 2;
    }
    if(pthread_create(&thread,NULL,churn,NULL))return 3;
    while(!atomic_load(&ready))sched_yield();
    for(int i=0;i<40;i++) {
        if(pipe(channel)){failures++;break;}
        pid_t child=vfork();
        if(child==0)_exit(write(channel[1],"X",1)==1?0:121);
        if(child<0){failures++;close(channel[0]);close(channel[1]);break;}
        errno=0;int closed=close(channel[1]);int saved_errno=errno;
        char byte=0,last=0;
        int n=closed==0?read(channel[0],&byte,1):-2;
        int eof=n==1?read(channel[0],&last,1):-2;
        pid_t waited=waitpid(child,&status,0);
        close(channel[0]);
        if(closed || n!=1 || byte!='X' || eof!=0 || waited!=child || status!=0) {
            printf("iteration=%d close=%d errno=%d read=%d byte=%d eof=%d wait=%ld status=%d\n",i,closed,saved_errno,n,byte,eof,(long)waited,status);
            failures++;break;
        }
    }
    atomic_store(&stopped,1);
    pthread_join(thread,NULL);
    for(int i=0;i<32;i++)if(close(held[i]))failures++;
    failures+=atomic_load(&errors);
    printf("FD_COPY_CLOSE_RACE failures=%d\n",failures);
    return failures?20:0;
}
