/* AROS vfork/exec descriptor isolation matrix; external deadline required. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
static int saved[2];
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--child"))return 0;
    if(argc!=3)return 2;
    const char *mode=argv[1];int p[2],status=0;
    if(strcmp(mode,"nofork") && strcmp(mode,"vfork-exit") && strcmp(mode,"exec") && strcmp(mode,"cloexec") && strcmp(mode,"vfork-fd"))return 2;
    if(pipe(p))return 3;
    saved[0]=p[0];saved[1]=p[1];
    int clo=!strcmp(mode,"cloexec");
    int inherited=!strcmp(mode,"vfork-fd");
    if(clo && fcntl(p[1],F_SETFD,FD_CLOEXEC))return 4;
    printf("before mode=%s p=%d,%d flags=%d,%d\n",mode,p[0],p[1],fcntl(p[0],F_GETFD),fcntl(p[1],F_GETFD));fflush(stdout);
    pid_t child=-1;
    if(strcmp(mode,"nofork")) {
        child=vfork();
        if(child==0) {
            if(inherited) {
                if(fcntl(saved[0],F_GETFD)<0 || fcntl(saved[1],F_GETFD)<0)_exit(121);
                _exit(write(saved[1],"X",1)==1?0:122);
            }
            if(!strcmp(mode,"vfork-exit"))_exit(0);
            close(saved[0]);
            if(!clo)close(saved[1]);
            execl(argv[2],argv[2],"--child",(char*)0);_exit(111);
        }
        if(child<0)return 5;
    }
    errno=0;int f0=fcntl(saved[0],F_GETFD);int e0=errno;
    errno=0;int f1=fcntl(saved[1],F_GETFD);int e1=errno;
    errno=0;int closed=close(saved[1]);int close_errno=errno;
    printf("after p=%d,%d saved=%d,%d flags=%d/%d,%d/%d close=%d errno=%d\n",p[0],p[1],saved[0],saved[1],f0,e0,f1,e1,closed,close_errno);fflush(stdout);
    int n=-2,tail=-2;unsigned char b=0;
    if(closed==0) {
        errno=0;n=read(saved[0],&b,1);int re=errno;
        printf("read=%d errno=%d byte=%u\n",n,re,(unsigned)b);fflush(stdout);
        if(inherited && n==1) {char last;tail=read(saved[0],&last,1);printf("tail=%d\n",tail);fflush(stdout);}
    }
    pid_t w=0;if(child>0) {errno=0;w=waitpid(child,&status,0);int we=errno;printf("wait=%ld errno=%d status=%d\n",(long)w,we,status);fflush(stdout);}
    close(saved[0]);return closed==0 && (inherited?(n==1 && b=='X' && tail==0):n==0) && (child<0 || (w==child && status==0))?0:7;
}
