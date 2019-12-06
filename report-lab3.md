# JOS Lab3 Report

陈仁泽 1700012774

[TOC]

## Exercise 1

在`kern/pmap.c`中加入：

```C
	//////////////////////////////////////////////////////////////////////
	// Make 'envs' point to an array of size 'NENV' of 'struct Env'.
	// LAB 3: Your code here.
    envs = (struct Env *) boot_alloc(NENV * sizeof(struct Env));
    memset(envs, 0, NENV * sizeof(struct Env));
```

```C
	//////////////////////////////////////////////////////////////////////
	// Map the 'envs' array read-only by the user at linear address UENVS
	// (ie. perm = PTE_U | PTE_P).
	// Permissions:
	//    - the new image at UENVS  -- kernel R, user R
	//    - envs itself -- kernel RW, user NONE
	// LAB 3: Your code here.
    boot_map_region(kern_pgdir, UENVS, PTSIZE, PADDR(envs), PTE_U);
```



## Exercise 2

### `env_init`

按要求初始化各个PCB以及空闲链表。

```C
void
env_init(void)
{
	// Set up envs array
	// LAB 3: Your code here.
    int i;
    struct Env *e;

    env_free_list = NULL;
    for (i = NENV - 1; i >= 0; --i) {
        e = &envs[i];
        e->env_id = 0;
        e->env_status = ENV_FREE;
        e->env_link = env_free_list;
        env_free_list = e;
    }

	// Per-CPU part of the initialization
	env_init_percpu();
}
```



### `env_setup_vm`

复制内核页目录，并设置自映射。

```c
static int
env_setup_vm(struct Env *e)
{
	struct PageInfo *pp = NULL;

	// Allocate a page for the page directory
	if (!(pp = page_alloc(ALLOC_ZERO)))
		return -E_NO_MEM;

	// Now, set e->env_pgdir and initialize the page directory.

	// LAB 3: Your code here.
    e->env_pgdir = page2kva(pp);
    pp->pp_ref += 1;

    for (size_t i = PDX(UTOP); i < NPDENTRIES; ++i)
        e->env_pgdir[i] = kern_pgdir[i];
	// UVPT maps the env's own page table read-only.
	// Permissions: kernel R, user R
    e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;
	return 0;
}
```



### `region_alloc`

按要求为进程的虚拟页分配对应的物理页即可。

```C
static void
region_alloc(struct Env *e, void *va, size_t len)
{
	// LAB 3: Your code here.
	// (But only if you need it for load_icode.)
    uintptr_t vstart, vend;
    struct PageInfo *pp;
    int err;

    vstart = ROUNDDOWN((uintptr_t)va, PGSIZE);
    vend = ROUNDUP((uintptr_t)va + len, PGSIZE);

    for (; vstart < vend; vstart += PGSIZE) {
        if (!(pp = page_alloc(ALLOC_ZERO)))
            panic("region_alloc(1)");
        if ((err = page_insert(e->env_pgdir, pp, (void*)vstart, PTE_W | PTE_U)) < 0)
            panic("region_alloc(2): %e", err);
    }
}
```



### `load_icode`

按要求加载即可。有一些需要注意的点：

- 在加载前，可以先将页目录替换为进程的页目录。在加载完成后，再替换回内核页目录。
- 进程trapframe的eip设置为程序入口，这样进程回到用户态后就会从入口处开始执行。

```C
static void
load_icode(struct Env *e, uint8_t *binary)
{
	// LAB 3: Your code here.
    struct Elf *eh;
    struct Proghdr *ph, *ph_end;

    eh = (struct Elf *) binary;
    if (eh->e_magic != ELF_MAGIC)
        panic("load_icode(1)");

    ph = (struct Proghdr *) (binary + eh->e_phoff);
    ph_end = ph + eh->e_phnum;

    lcr3(PADDR(e->env_pgdir));

    for (; ph < ph_end; ++ph) {
        if (ph->p_type != ELF_PROG_LOAD)
            continue;
        if (ph->p_filesz > ph->p_memsz)
            panic("load_icode(2)");

        region_alloc(e, (void*)ph->p_va, ph->p_memsz);

        memset((void*)ph->p_va, 0, ph->p_memsz);
        memcpy((void*)ph->p_va, binary + ph->p_offset, ph->p_filesz);
    }
    e->env_tf.tf_eip = eh->e_entry;
    
	// Now map one page for the program's initial stack
	// at virtual address USTACKTOP - PGSIZE.

	// LAB 3: Your code here.
    region_alloc(e, (void*)(USTACKTOP - PGSIZE), PGSIZE);

    lcr3(PADDR(kern_pgdir));
}
```



### `env_create`

分配一个PCB，加载程序，设置进程类型即可。

```C
void
env_create(uint8_t *binary, enum EnvType type)
{
	// LAB 3: Your code here.
    struct Env *e;
    int err;

    if ((err = env_alloc(&e, 0)) < 0)
        panic("env_create: %e", err);

    load_icode(e, binary);
    e->env_type = ENV_TYPE_USER;
}
```



### `env_run`

按要求设置curenv以及PCB的状态即可。

```C
void
env_run(struct Env *e)
{
	// Step 1: If this is a context switch (a new environment is running):
	//	   1. Set the current environment (if any) back to
	//	      ENV_RUNNABLE if it is ENV_RUNNING (think about
	//	      what other states it can be in),
	//	   2. Set 'curenv' to the new environment,
	//	   3. Set its status to ENV_RUNNING,
	//	   4. Update its 'env_runs' counter,
	//	   5. Use lcr3() to switch to its address space.
	// Step 2: Use env_pop_tf() to restore the environment's
	//	   registers and drop into user mode in the
	//	   environment.

	// LAB 3: Your code here.

	// panic("env_run not yet implemented");
    if (curenv && curenv->env_status == ENV_RUNNING)
        curenv->env_status = ENV_RUNNABLE;
    curenv = e;
    curenv->env_status = ENV_RUNNING;
    curenv->env_runs += 1;
    lcr3(PADDR(curenv->env_pgdir));
    env_pop_tf(&curenv->env_tf);
}
```

### 

## Exercise 4 & Challenge: Meta-Programming

### `kern/trapentry.inc`

新建一个文件`kern/trapentry.inc`，列出以下各项。`TH(n)`表示不会产生error-code的第n号中断的trap handler；`THE(n)`表示会产生error-code的第n号中断的trap handler。

```C
TH(0)
TH(1)
TH(2)
TH(3)
TH(4)
TH(5)
TH(6)
TH(7)
THE(8)
THE(10)
THE(11)
THE(12)
THE(13)
THE(14)
TH(16)
THE(17)
TH(18)
TH(19)
TH(48)
```



### `kern/trapentry.S`

在`kern/trapentry.S`中定义`TH`和`THE`，引入`kern/trapentry.inc`，构成各个中断处理例程：

```assembly

#define TH(n) \
TRAPHANDLER_NOEC(handler##n, n)

#define THE(n) \
TRAPHANDLER(handler##n, n)

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
#include <kern/trapentry.inc>
```



### `_alltraps`

先补齐trapframe所需要的信息，更改段寄存器，接着将ESP压栈作为参数`struct Trapframe* tf`并调用`trap`函数。

```assembly
/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
    pushl %ds 
    pushl %es 
    pushal 

    movw $GD_KD, %ax
    movw %ax, %ds 
    movw %ax, %es

    pushl %esp 
    call trap 
trap_spin:
    jmp trap_spin
```



### `kern/trap.c`

在`kern/trap.c`中定义`TH`和`THE`，引入`kern/trapentry.inc`，构成中断向量表：

```C
#define TH(n) extern void handler##n (void);
#define THE(n) TH(n)

#include <kern/trapentry.inc>

#undef THE
#undef TH

#define TH(n) [n] = handler##n,
#define THE(n) TH(n)

static void (* handlers[256])(void) = {
#include <kern/trapentry.inc>
};

#undef THE
#undef TH
```



### `trap_init`

考虑到处理的方便，将所有的中断向量都设为中断门，也就是处理过程中屏蔽外部中断。

```C
void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
    for (int i = 0; i < 32; ++i) 
        SETGATE(idt[i], 0, GD_KT, handlers[i], 0);
    SETGATE(idt[T_BRKPT], 0, GD_KT, handlers[T_BRKPT], 3);
    SETGATE(idt[T_SYSCALL], 0, GD_KT, handlers[T_SYSCALL], 3);
	// Per-CPU setup 
	trap_init_percpu();
}
```



### Questions

#### 1.

> What is the purpose of having an individual handler function
> for each exception/interrupt? (i.e., if all exceptions/interrupts
> were delivered to the same handler, what feature that exists in
> the current implementation could not be provided?)

因为有的中断硬件会多压一个错误码，采用分立的handler目的在于处理这种不一致以提供一致的trapframe。



#### 2.

> Did you have to do anything to make the user/softint program
> behave correctly? The grade script expects it to produce a
> general protection fault (trap 13), but softint's code says
> int \$14. Why should this produce interrupt vector 13? What
> happens if the kernel actually allows softint's int \$14
> instruction to invoke the kernel's page fault handler (which is
> interrupt vector 14)?

因为14号中断向量的DPL为0，即内核特权级。根据x86 ISA的说明（https://www.felixcloutier.com/x86/intn:into:int3:int1）：

```
IF software interrupt (* Generated by INT n, INT3, or INTO; does not apply to INT1 *)
        THEN
            IF gate DPL < CPL (* PE = 1, DPL < CPL, software interrupt *)
                THEN #GP(error_code(vector_number,1,0)); FI;
                (* idt operand to error_code set because vector is used *)
                (* ext operand to error_code is 0 because INT n, INT3, or INTO*)
```

在用户态使用`int $14`，会触发保护异常（Gerenal Protection Fault，伪代码中的GP）。

如果内核允许用户主动触发缺页异常，将会导致严重的不一致性，内核将难以辨识用户态触发的缺页异常到底因何发生。



## Exercise 5

简答地识别trapno并派发即可：

```C
    switch(tf->tf_trapno) {
    case T_PGFLT: {
        page_fault_handler(tf);
        return;
    }
    default:
        break;
    }
```



## Exercise 6

在`switch`语句中添加一个`case`即可：

```C
    case T_BRKPT: {
        monitor(tf);
        return;
    }
```



### Questions

#### 3.

> The break point test case will either generate a break point
> exception or a general protection fault depending on how you
> initialized the break point entry in the IDT (i.e., your call to
> SETGATE from trap_init). Why? How do you need to set it up in
> order to get the breakpoint exception to work as specified
> above and what incorrect setup would cause it to trigger a
> general protection fault?

该test使用`int $3`指令触发断点异常，因此3号中断向量的DPL必须设为3，即用户特权级。如果没有如此设置，将会触发保护异常。

#### 4.

> What do you think is the point of these mechanisms,
> particularly in light of what the user/softint test program does?

意义在于防止用户随意地触发异常，但同时又留出一个接口供用户使用系统服务。



## Exercise 7

### syscall中断向量

syscall中断向量的建立和初始化之前已有涉及，便不再赘述。



### `trap_dispatch`

在`trap_dispatch`中的`switch`语句添加一个`case`，通过trapframe获得syscall参数，并设置返回值。

```C
    case T_SYSCALL: {
        // eax, edx, ecx, ebx, edi, esi;
        struct PushRegs *r = &tf->tf_regs;
        r->reg_eax = syscall(
            r->reg_eax, 
            r->reg_edx, 
            r->reg_ecx, 
            r->reg_ebx, 
            r->reg_edi, 
            r->reg_esi
        );
        return;
    }
```



### `syscall`

补全`kern/syscall.c`中的`syscall`函数，注意采用`(void)x`的形式“使用”一下`x`变量，防止编译器报错（因为开了`-Werror`……）：

```C
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	// panic("syscall not implemented");
    (void)syscallno, (void)a1, (void)a2, (void)a3, (void)a4, (void)a5;
	switch (syscallno) {
    case SYS_cputs: {
        sys_cputs((const char *)a1, (size_t)a2);
        return 0;
    }
    case SYS_cgetc: {
        return sys_cgetc();
    }
    case SYS_env_destroy: {
        return sys_env_destroy((envid_t)a1);
    }
    case SYS_getenvid: {
        return sys_getenvid();
    }
	default:
		return -E_INVAL;
	}
}
```



## Exercise 8

添加如下语句即可：

```C
	thisenv = &envs[ENVX(sys_getenvid())];
```



## Exercise 9

### `page_fault_handler`

在`page_fault_handler`中添加：

```C
    if ((tf->tf_cs & 3) == 0)
        panic("page fault in kernel mode");
```



### `user_mem_check`

实现较为简单，注意要将`user_mem_check_addr`设置为合适的值。

```C
int
user_mem_check(struct Env *env, const void *va, size_t len, int perm)
{
	// LAB 3: Your code here.
    pte_t *pte;
    uintptr_t vstart, vend;

    vstart = ROUNDDOWN((uintptr_t)va, PGSIZE);
    vend = ROUNDUP((uintptr_t)va + len, PGSIZE);
    
    if (vend > ULIM) {
        user_mem_check_addr = MAX(ULIM, (uintptr_t)va);
        return -E_FAULT;
    }
    for (; vstart < vend; vstart += PGSIZE) {
        pte = pgdir_walk(env->env_pgdir, (void*)vstart, 0);
        if (!pte || (*pte & (perm | PTE_P)) != (perm | PTE_P)) {
            user_mem_check_addr = MAX(vstart, (uintptr_t)va);
            return -E_FAULT;
        }
    }

	return 0;
}
```



### `kern/syscall.c`

当前的syscall只有`cons_cputs`有涉及到访存，故只需在其中加入内存检查即可：

```C
    user_mem_assert(curenv, s, len, PTE_U);
```



### `debuginfo_eip`

加入如下检查语句：

```C
		// Make sure this memory is valid.
		// Return -1 if it is not.  Hint: Call user_mem_check.
		// LAB 3: Your code here.
        if (curenv && 
            	user_mem_check(curenv, (void*)usd, 
                               sizeof(struct UserStabData), PTE_U) < 0)
            return -1;
```

```C
		// Make sure the STABS and string table memory is valid.
		// LAB 3: Your code here.
        if (curenv && (
                user_mem_check(curenv, (void*)stabs, 
                               (uintptr_t)stab_end - (uintptr_t)stabs, PTE_U) < 0 || 
                user_mem_check(curenv, (void*)stabstr, 
                               (uintptr_t)stabstr_end - (uintptr_t)stabstr, PTE_U) < 0))
            return -1;
```



## This completes this lab

```
divzero: OK (3.4s) 
softint: OK (1.0s) 
badsegment: OK (0.9s) 
Part A score: 30/30

faultread: OK (1.7s) 
faultreadkernel: OK (1.2s) 
faultwrite: OK (1.8s) 
faultwritekernel: OK (1.2s) 
breakpoint: OK (1.7s) 
testbss: OK (2.1s) 
hello: OK (2.1s) 
buggyhello: OK (1.8s) 
buggyhello2: OK (1.2s) 
evilhello: OK (1.8s) 
Part B score: 50/50

Score: 80/80
```



## Challenge: Breakpoint

x86在EFLAGS的TF位置位时，每执行一条指令都会触发一次调试异常（触发后会复位TF）。可以借此实现断点续行。

首先在`trap_dispatch`中处理调试异常（和断点异常处理一致）：

```C
    case T_DEBUG:
    case T_BRKPT: {
        monitor(tf);
        return;
    }
```

接着在monitor中添加两个命令：`step`、`continue`，用于步进和继续执行。简单地检查一下是否是因为用户态的断点/调试异常触发的，接着设置trapframe的EFLAGS的TF位：

```C

int mon_continue(int argc, char **argv, struct Trapframe *tf) {
    if (!(tf && (tf->tf_trapno == T_DEBUG || tf->tf_trapno == T_BRKPT) && 
          ((tf->tf_cs & 3) == 3)))
        return 0;
    tf->tf_eflags &= ~FL_TF;
    return -1;
}

int mon_step(int argc, char **argv, struct Trapframe *tf) {
    if (!(tf && (tf->tf_trapno == T_DEBUG || tf->tf_trapno == T_BRKPT) && 
          ((tf->tf_cs & 3) == 3)))
        return 0;
    tf->tf_eflags |= FL_TF;
    return -1;
}
```



### 测试效果

运行`breakpoint`程序的结果

```
[00000000] new env 00001000
Incoming TRAP frame at 0xefffffbc
Incoming TRAP frame at 0xefffffbc
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
TRAP frame at 0xf01c7000
  edi  0x00000000
  esi  0x00000000
  ebp  0xeebfdfc0
  oesp 0xefffffdc
  ebx  0x00802000
  edx  0x0080202c
  ecx  0x00000000
  eax  0xeec00000
  es   0x----0023
  ds   0x----0023
  trap 0x00000003 Breakpoint
  err  0x00000000
  eip  0x00800037
  cs   0x----001b
  flag 0x00000082
  esp  0xeebfdfc0
  ss   0x----0023
K> step
Incoming TRAP frame at 0xefffffbc
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
TRAP frame at 0xf01c7000
  edi  0x00000000
  esi  0x00000000
  ebp  0xeebfdff0
  oesp 0xefffffdc
  ebx  0x00802000
  edx  0x0080202c
  ecx  0x00000000
  eax  0xeec00000
  es   0x----0023
  ds   0x----0023
  trap 0x00000001 Debug
  err  0x00000000
  eip  0x00800038
  cs   0x----001b
  flag 0x00000182
  esp  0xeebfdfc4
  ss   0x----0023
K> continue
Incoming TRAP frame at 0xefffffbc
[00001000] exiting gracefully
[00001000] free env 00001000
Destroyed the only environment - nothing more to do!
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
K> 
```

