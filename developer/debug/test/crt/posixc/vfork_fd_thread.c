/* Isolate descriptor/exec contracts when called from a pthread. */
#define main matrix_main
#include "vfork_fd.c"
#undef main
#include <pthread.h>
#include <stdint.h>
struct input {int argc; char **argv;};
static void *worker(void *raw) {
    struct input *in=raw;
    return (void *)(intptr_t)matrix_main(in->argc,in->argv);
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--child"))return 0;
    struct input in={argc,argv};pthread_t t;void *result;
    int rc=pthread_create(&t,NULL,worker,&in);
    if(rc){printf("pthread_create=%d\n",rc);return 9;}
    rc=pthread_join(t,&result);
    if(rc){printf("pthread_join=%d\n",rc);return 10;}
    return (int)(intptr_t)result;
}
