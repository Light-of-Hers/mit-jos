// check the xstack overflow

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
    envid_t eid;
    envid_t peid;

    peid = sys_getenvid();
    // this will be ok
    for (size_t i = 0; i < 10; ++i) {
        if (fork() < 0)
            cprintf("cannot fork\n");
        sys_yield();
    }
    if (sys_getenvid() != peid)
        exit();
    // this will be bad
    for (size_t i = 0; i < 20; ++i) {
        if (fork() < 0)
            cprintf("cannot fork\n");
        sys_yield();
    }
}
