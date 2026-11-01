/* Native fd.library retained-reservation regression (version 51 required). */
#include <proto/exec.h>
#include <proto/fd.h>
#include <libraries/fd.h>
#include <errno.h>
#include <stdio.h>
struct Library *FDBase;
#define CHECK(expr) do {if (!(expr)) {printf("FAIL line %d: %s\n",__LINE__,#expr);return 20;}} while (0)
int main(void)
{
    LONG fd=-1, next=-1;
    FDBase=OpenLibrary((CONST_STRPTR)"fd.library",51);
    if (!FDBase) {puts("fd.library 51 unavailable");return 77;}
    printf("fd.library %u.%u\n",FDBase->lib_Version,FDBase->lib_Revision);
    CHECK(FD_Alloc(64,FD_OWNER_POSIXC,NULL,&fd)==0);
    CHECK(FD_Reserve(fd,FD_OWNER_POSIXC,NULL)==EBUSY);
    CHECK(FD_Retain(fd,FD_OWNER_BSDSOCKET)==EBADF);
    CHECK(FD_Retain(fd,FD_OWNER_POSIXC)==0);
    CHECK(FD_Free(fd,FD_OWNER_POSIXC)==0);
    CHECK(FD_GetOwner(fd)==FD_OWNER_POSIXC);
    CHECK(FD_Check(fd)==EBUSY);
    CHECK(FD_Free(fd,FD_OWNER_POSIXC)==0);
    CHECK(FD_Check(fd)==0);
    CHECK(FD_Retain(fd,FD_OWNER_POSIXC)==EBADF);
    CHECK(FD_Alloc(fd,FD_OWNER_BSDSOCKET,NULL,&next)==0 && next==fd);
    CHECK(FD_Free(next,FD_OWNER_BSDSOCKET)==0);
    CloseLibrary(FDBase);
    puts("FD_RETAIN_PASS");
    return 0;
}
