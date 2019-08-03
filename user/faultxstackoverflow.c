// check the xstack overflow

#include <inc/lib.h>

void pgfault(struct UTrapframe *utf) {
    asm volatile(
        "loop: push %%eax\n"
        "jmp loop"
        :
    );
}

void
umain(int argc, char **argv)
{
    set_pgfault_handler(pgfault);
	*(int*)0 = 0;
}
