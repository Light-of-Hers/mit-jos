// program to cause a breakpoint trap

#include <inc/lib.h>
#include <inc/stdio.h>

void
umain(int argc, char **argv)
{
	asm volatile("int $3");
}

