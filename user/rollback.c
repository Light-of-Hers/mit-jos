#include <inc/lib.h>

void 
umain(int argc, char** argv)
{
    uint32_t dmail = 0;
    int time = 0;
    envid_t spst = sys_env_snapshot(&dmail);
    if (dmail < 10) {
        cprintf("recv dmail %u in time %d\n", dmail++, time++);
        sys_env_rollback(spst, dmail);
    }
}