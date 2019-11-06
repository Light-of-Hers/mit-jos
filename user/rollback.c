#include <inc/lib.h>

void 
umain(int argc, char** argv)
{
    int x = 0;
    envid_t spst = sys_env_snapshot();
    cprintf("x = %d\n", x++);
    sys_env_rollback(spst);
}