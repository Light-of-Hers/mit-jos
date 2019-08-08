#include "ns.h"
#include <inc/log.h>

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
    int r;
    int feid;

    while(1) {
        if (r = ipc_recv(&feid, &nsipcbuf, NULL), r < 0)
            panic("in output, ipc_recv: %e", r);
        if (r != NSREQ_OUTPUT || feid != ns_envid)
            continue;
        while (r = sys_net_transmit(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len), r < 0)
            sys_yield();
    }
}
