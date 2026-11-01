/* Finite regression: nonblocking pipe IO must never leave a pending request. */
#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
static int enable(int fd) {int f=fcntl(fd,F_GETFL);return f<0?-1:fcntl(fd,F_SETFL,f|O_NONBLOCK);}
static int again(int n) {return n==-1 && (errno==EAGAIN || errno==EWOULDBLOCK);}
int main(void) {
 int p[2];char b[8192];memset(b,'X',sizeof(b));
 if(pipe(p))return 2;
 if(enable(p[0])||enable(p[1])) {printf("NONBLOCK_UNAVAILABLE errno=%d\n",errno);close(p[0]);close(p[1]);return 77;}
 errno=0;int n=read(p[0],b,sizeof(b));if(!again(n))return 10;
 int alias=dup(p[0]);if(alias<0||!(fcntl(alias,F_GETFL)&O_NONBLOCK))return 11;
 if(write(p[1],"X",1)!=1 || read(alias,b,sizeof(b))!=1 || b[0]!='X')return 12;
 int total=0;
 for(int i=0;i<4096;i++) {errno=0;n=write(p[1],b,sizeof(b));if(again(n))break;if(n<=0)return 13;total+=n;}
 if(!again(n) || total==0)return 14;
 if(read(p[0],b,1)!=1)return 19;
 errno=0;n=write(p[1],"YZ",2);if(!again(n))return 20;
 if(read(p[0],b,sizeof(b))<=0 || write(p[1],"Y",1)!=1)return 15;
 while((n=read(p[0],b,sizeof(b)))>0){}
 if(!again(n))return 16;
 close(p[1]);if(read(alias,b,sizeof(b))!=0)return 17;
 if(fcntl(alias,F_SETFL,fcntl(alias,F_GETFL)&~O_NONBLOCK) || (fcntl(p[0],F_GETFL)&O_NONBLOCK))return 18;
 close(alias);close(p[0]);
#ifdef __AROS__
 DIR *dir=opendir("RAM:");
#else
 DIR *dir=opendir(".");
#endif
 if(!dir)return 21;
 int fd=dirfd(dir),flags=fcntl(fd,F_GETFL);
 if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK))return 22;
 closedir(dir);printf("PIPE_NONBLOCK_PASS capacity=%d empty/partial/full/progress/dup/EOF/toggle\n",total);return 0;
}
