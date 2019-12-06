#include <inc/lib.h>

int min = 0;

void 
umain(int argc, char** argv)
{
    uint32_t dmail = 0;
    int sec = 0;
    envid_t spst = sys_env_snapshot(&dmail);
    if (dmail == EMPTY_DMAIL) {
        sys_env_rollback(spst, 0);
    } else if (dmail < 10) {
        cprintf("recv dmail %d at time(%d:%d)\n", dmail, min++, sec++);
        sys_env_rollback(spst, ++dmail);
    }
}