#include <inc/lib.h>

void 
umain(int argc, char** argv)
{
    envid_t eid;
    
    if (eid = fork(), eid > 0) {
        for (int i = 1; i <= 10; ++i) {
            ipc_send(eid, i, 0, 0);
        }
        ipc_send(eid, 0, 0, 0);
    } else if (eid == 0) {
        int time = 0;
        envid_t spst = sys_env_snapshot();
        int value;
        if (time++, value = ipc_recv(0, 0, 0), value > 0) {
            cprintf("recv %d in time %d\n", value, time);
            cprintf("rollback!!!\n");
            sys_env_rollback(spst);
        }
    } else {
        panic("???");
    }
}