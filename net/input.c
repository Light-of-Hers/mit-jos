#include "ns.h"
#include <inc/log.h>

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	int r;

	while(1) {
		if (r = sys_page_alloc(0, &nsipcbuf, PTE_P | PTE_U | PTE_W), r < 0)
			panic("in input, sys_page_alloc: %e", r);
		while (r = sys_dl_receive(nsipcbuf.pkt.jp_data, PGSIZE - sizeof(int)), r < 0)
			sys_yield();
		nsipcbuf.pkt.jp_len = r;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_U);
		if (r = sys_page_unmap(0, &nsipcbuf), r < 0)
			panic("in input, sys_page_unmap: %e", r);
	}
}
