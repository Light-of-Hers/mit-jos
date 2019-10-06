// program to cause a breakpoint trap

#include <inc/lib.h>
#include <inc/stdio.h>

void
umain(int argc, char **argv)
{
	asm volatile("int $3");
	for (int i = 0; i < 10; ++i) 
		cprintf("%d\n", i);
}

