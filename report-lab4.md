# JOS Lab4 Report

陈仁泽 1700012774

[TOC]

## Exercise 1

调用`boot_map_region`即可，注意需要检查范围是否超过`MMIOLIM`：

```C
void *
mmio_map_region(physaddr_t pa, size_t size)
{
	static uintptr_t base = MMIOBASE;

    uintptr_t start;
    
    start = base;
    size = ROUNDUP(size, PGSIZE);
	if (base + size > MMIOLIM)
		panic("overflow MMIOLIM");
    base += size;
    boot_map_region(kern_pgdir, start, size, pa, PTE_W | PTE_PCD | PTE_PWT);
    return (void*)start;
}
```



## Exercise 2

在`page_init`里不将`MPENTRY_PADDR`加入空闲链表即可：

```C
void
page_init(void)
{
#define MARK_FREE(_i) do {\
    size_t __tmp__ = _i;\
    pages[__tmp__].pp_ref = 0;\
    pages[__tmp__].pp_link = page_free_list;\
	page_free_list = &pages[__tmp__];\
} while(0)
#define MARK_USE(_i) do {\
    size_t __tmp__ = _i;\
    pages[__tmp__].pp_ref = 0;\
    pages[__tmp__].pp_link = NULL;\
} while(0)

    extern char end[];
    physaddr_t bss_end;
    physaddr_t boot_alloc_end;
	size_t i;

    page_free_list = NULL;
    bss_end = PADDR(ROUNDUP((char*)end, PGSIZE));
    boot_alloc_end = PADDR(boot_alloc(0));

    MARK_USE(0);
    // --- 添加的内容 ---
    for (i = 1; i < MPENTRY_PADDR / PGSIZE; ++i)
        MARK_FREE(i);
    MARK_USE(i++);
    // -----------------
    for (; i < npages_basemem; ++i)
        MARK_FREE(i);
    for (; i < EXTPHYSMEM / PGSIZE; ++i)
        MARK_USE(i);
    for (; i < bss_end / PGSIZE; ++i)
        MARK_USE(i);
    for (; i < boot_alloc_end / PGSIZE; ++i)
        MARK_USE(i);
    for (; i < npages; ++i)
        MARK_FREE(i);
	log_int(npages);

#undef MARK_USE
#undef MARK_FREE
}
```

> Q: Compare kern/mpentry.S side by side with boot/boot.S. Bearing in mind that kern/mpentry.S is compiled and linked to run above KERNBASE just like everything else in the kernel, what is the purpose of macro MPBOOTPHYS? Why is it necessary in kern/mpentry.S but not in boot/boot.S? In other words, what could go wrong if it were omitted in kern/mpentry.S? Hint: recall the differences between the link address and the load address that we have discussed in Lab 1.

> A: 
> 1. 计算出符号的物理地址。
> 2. 该代码段在编译链接时所重定位的位置不在`MPENTRY_PADDR`这块。实际运行时将这段代码拷贝到`MPENTRY_PADDR`再运行AP。因此对于需要绝对寻址的代码，要用`MPBOOTPHYS`来修改地址。而`boot.S`就没有这个需求。



## Exercise 3

直接用`boot_map_region`分配就行了：

```C
static void
mem_init_mp(void)
{
    for (size_t i = 0; i < NCPU; ++i) {
        boot_map_region(
            kern_pgdir, 
            KSTACKTOP - KSTKSIZE - i * (KSTKSIZE + KSTKGAP),
            KSTKSIZE,
            PADDR(percpu_kstacks[i]),
            PTE_W 
        );
    }
}
```



## Exercise 4

先获取当前的CPU号，再初始化TS的ESP,SS等部分，初始化GDT中的TSS项，最后加载TSS选择子和IDT。

```C
void
trap_init_percpu(void)
{
    struct Taskstate *ts;
    size_t i;

    i = cpunum();
    ts = &cpus[i].cpu_ts;
    ts->ts_esp0 = (uintptr_t)percpu_kstacks[i] + KSTKSIZE;
    ts->ts_ss0 = GD_KD;
    ts->ts_iomb = sizeof(struct Taskstate);

    gdt[(GD_TSS0 >> 3) + i] = SEG16(STS_T32A, (uint32_t)ts, sizeof(struct Taskstate) - 1, 0);
    gdt[(GD_TSS0 >> 3) + i].sd_s = 0;

    ltr(GD_TSS0 + (i << 3));

    lidt(&idt_pd);
}
```



## Exercise 5

在`kern/init.c`的`i386_init`中加锁：

```C
	// Lab 4 multitasking initialization functions
	pic_init();

	// Acquire the big kernel lock before waking up APs
	// Your code here:
    lock_kernel();

	// Starting non-boot CPUs
	boot_aps();
```

在`kern/init.c`的`mp_main`中加锁：

```C
	// Now that we have finished some basic setup, call sched_yield()
	// to start running processes on this CPU.  But make sure that
	// only one CPU can enter the scheduler at a time!
	//
	// Your code here:
    lock_kernel();

    sched_yield();
```

在`kern/trap.c`的`trap`中加锁：

```C
	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
        lock_kernel();
```

在`kern/env.c`的`env_run`中解锁：

```C
    unlock_kernel();
    env_pop_tf(&e->env_tf);
```

> Q: It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

> A: 内核锁的获取是在陷入内核之后进行的，这样的话在抢夺内核锁之前，内核栈上至少已经压入了CS和EIP等硬件压入的上下文。如果共享内核栈的话，在多个CPU同时陷入内核时，就会造成内核栈上保存的内容不一致。



## Exercise 6

按要求实现即可，因为有内核锁，所以完全不用考虑竞争的问题(^_^)

```C
void
sched_yield(void)
{
	struct Env *idle;
    
    idle = NULL;
    if (curenv) {
        size_t eidx = ENVX(curenv->env_id);
        uint32_t mask = NENV - 1;
        for (size_t i = (eidx + 1) & mask; i != eidx; i = (i + 1) & mask) {
            if (envs[i].env_status == ENV_RUNNABLE) {
                idle = &envs[i];
                break;
            }
        }
        if (!idle && curenv->env_status == ENV_RUNNING)
            idle = curenv;
    } else {
        for (size_t i = 0; i < NENV; ++i) {
            if (envs[i].env_status == ENV_RUNNABLE) {
                idle = &envs[i];
                break;
            }
        }
    }
    if (idle)
        env_run(idle);
	// sched_halt never returns
    sched_halt();
}
```

对syscall等的修改比较简单，就不列出了

> Q: In your implementation of env_run() you should have called lcr3(). Before and after the call to lcr3(), your code makes references (at least it should) to the variable e, the argument to env_run. Upon loading the %cr3 register, the addressing context used by the MMU is instantly changed. But a virtual address (namely e) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer e be dereferenced both before and after the addressing switch?

> A: 因为所有进程的地址空间在内核部分的映射都是一样的。

> Q: Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

> A: 因为进程没有自己单独的内核栈，因此寄存器上下文必须保存在PCB之类的结构中，以便之后恢复上下文。在`trap.c`的`trap`内保存寄存器上下文：
>
> ```C
> 		// Copy trap frame (which is currently on the stack)
> 		// into 'curenv->env_tf', so that running the environment
> 		// will restart at the trap point.
> 		curenv->env_tf = *tf;
> 		// The trapframe on the stack should be ignored from here on.
> 		tf = &curenv->env_tf;
> ```



## Exercise 7

### `sys_exofork`

按要求实现即可：

```C
static envid_t
sys_exofork(void)
{
    int r;
    struct Env *parent, *child;
    
    parent = curenv;
    if (r= env_alloc(&child, parent->env_id), r < 0)
        return r;

    child->env_status = ENV_NOT_RUNNABLE;
    child->env_tf = parent->env_tf;
    child->env_tf.tf_regs.reg_eax = 0;

    return child->env_id;
}
```



### `sys_env_set_status`

需要检查`status`是合法的，并且`envid2env`也要检查合法性：

```C
static int
sys_env_set_status(envid_t envid, int status)
{
	int r;
    struct Env *e;

    if (status < 0 || status > ENV_NOT_RUNNABLE)
        return -E_INVAL;
    if (r = envid2env(envid, &e, 1), r < 0)
        return r;
    
    e->env_status = status;
    return 0;
}
```



### `sys_page_alloc`

注意需要检查地址的合法性和对齐性，以及`perm`的合法性：

```C
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
#define CHECK_ALIGIN_UVA(_va) do {\
    uintptr_t __tmp__ = (uintptr_t)(_va);\
    if (__tmp__ >= UTOP || (__tmp__ & (PGSIZE - 1)))\
        return -E_INVAL;\
} while (0)
#define CHECK_SYSCALL_PERM(_perm) do {\
    int __tmp__ = _perm;\
    if (!(__tmp__ & PTE_P) || !(__tmp__ & PTE_U) || (__tmp__ & ~PTE_SYSCALL))\
        return -E_INVAL;\
} while (0)
   	int r;
    struct Env *e;
    struct PageInfo *pp;

    CHECK_ALIGIN_UVA(va);
    CHECK_SYSCALL_PERM(perm);
    if (r = envid2env(envid, &e, 1), r < 0)
        return r;
    if (!(pp = page_alloc(ALLOC_ZERO)))
        return -E_NO_MEM;
    if (r = page_insert(e->env_pgdir, pp, va, perm), r < 0) {
        page_free(pp);
        return r;
    }
    return 0;
}
```



### `sys_page_map`

过程较为繁琐，需要进行多个检查：

```C
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	int r;
    struct Env *srce, *dste;
    pte_t *pte;
    struct PageInfo *pp;

    CHECK_ALIGIN_UVA(srcva);
    CHECK_ALIGIN_UVA(dstva);
    CHECK_SYSCALL_PERM(perm);
    if (r = envid2env(srcenvid, &srce, 1), r < 0)
        return r;
    if (r = envid2env(dstenvid, &dste, 1), r < 0)
        return r;
    if (!(pp = page_lookup(srce->env_pgdir, srcva, &pte)))
        return -E_INVAL;
    if (!(*pte | PTE_W) && (perm & PTE_W))
        return -E_INVAL;
    if (r = page_insert(dste->env_pgdir, pp, dstva, perm), r < 0)
        return r;
    return 0;
}
```



### `sys_page_unmap`

比较简单，用`page_remove`即可：

```C
static int
sys_page_unmap(envid_t envid, void *va)
{
    int r;
    struct Env *e;

    CHECK_ALIGIN_UVA(va);
    if (r = envid2env(envid, &e, 1), r < 0)
        return r;
    page_remove(e->env_pgdir, va);
    return 0;
}
#undef CHECK_SYSCALL_PERM
#undef CHECK_ALIGIN_UVA
```



## Exercise 8

`envid2env`需要检查进程访问权限，其他按要求实现即可：

```C
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
    int r;
    struct Env *e;
    if (r = envid2env(envid, &e, 1), r < 0)
        return r;
    e->env_pgfault_upcall = func;
    return 0;
}
```



## Exercise 9

过程较为繁琐，具体过程见注释：

```C
void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
    if ((tf->tf_cs & 3) == 0) {
        print_trapframe(tf);
        panic("[%08x] kernel fault va %08x ip %08x\n",
		    curenv->env_id, fault_va, tf->tf_eip);
    }

    // check upcall
    if (!curenv->env_pgfault_upcall) {
        log("no upcall");
        goto bad;
    }
    // check whether upcall and xstack are in user space
    user_mem_assert(curenv, (void*)curenv->env_pgfault_upcall, 0, PTE_U);
    user_mem_assert(curenv, (void*)(UXSTACKTOP - 1), 0, PTE_U | PTE_W);
    // check xstack overflow
    if (fault_va < UXSTACKTOP - PGSIZE && fault_va >= UXSTACKTOP - 2 * PGSIZE) {
        log("xstack overflow");
        goto bad;
    }

    // prepare stack frame
    uint32_t esp;
    struct UTrapframe* utf;

    if (tf->tf_esp >= UXSTACKTOP - PGSIZE && tf->tf_esp <= UXSTACKTOP) {
        esp = tf->tf_esp - (sizeof(struct UTrapframe) + 4);
    } else {
        esp = UXSTACKTOP - sizeof(struct UTrapframe);
    }
    // stack overflow
    if (esp < UXSTACKTOP - PGSIZE) {
        log("xstack overflow");
        goto bad;
    }
    
    utf = (struct UTrapframe *) esp;
    utf->utf_eflags = tf->tf_eflags;
    utf->utf_eip = tf->tf_eip;
    utf->utf_esp = tf->tf_esp;
    utf->utf_regs = tf->tf_regs;
    utf->utf_err = tf->tf_err;
    utf->utf_fault_va = fault_va;

    // change esp / eip
    tf->tf_esp = esp;
    tf->tf_eip = (uint32_t)curenv->env_pgfault_upcall;
    return;

bad:
	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}
```



## Exercise 10

首先需要将返回地址压到“将要切换回去的栈”，接着依次恢复寄存器即可。

```C
.text
.globl _pgfault_upcall
_pgfault_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _pgfault_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	
	// LAB 4: Your code here.
    movl 48(%esp), %eax
    movl 40(%esp), %ebx
    subl $4, %eax
    movl %ebx, (%eax)
    movl %eax, 48(%esp)

	// Restore the trap-time registers.  After you do this, you
	// can no longer modify any general-purpose registers.
	// LAB 4: Your code here.
    addl $8, %esp
    popal

	// Restore eflags from the stack.  After you do this, you can
	// no longer use arithmetic operations or anything else that
	// modifies eflags.
	// LAB 4: Your code here.
    addl $4, %esp 
    popf

	// Switch back to the adjusted trap-time stack.
	// LAB 4: Your code here.
    movl (%esp), %esp

	// Return to re-execute the instruction that faulted.
	// LAB 4: Your code here.
    ret
```



## Exercise 11

分配一个异常处理栈，再设置upcall函数即可：

```C
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
#define PANIC panic("set_pgfault_handler: %e", r)

	int r;
	if (_pgfault_handler == 0) {
		// First time through!
		// LAB 4: Your code here.
		// panic("set_pgfault_handler not implemented");
        
        if (r = sys_page_alloc(0, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W), r < 0)
            PANIC;
        if (r = sys_env_set_pgfault_upcall(0, _pgfault_upcall), r < 0)
            PANIC;
	}

	// Save handler pointer for assembly to call.
	_pgfault_handler = handler;
    return;
    
#undef PANIC
}
```



## Exercise 12

### `fork`

先`exofork`一个进程，之后父进程将用户空间用`duppage`拷贝过去，再单独分配一个异常栈，设置upcall，最后再设置子进程的状态：

```C
envid_t
fork(void)
{
#define PANIC panic("fork: %e", r)
	// LAB 4: Your code here.
	// panic("fork not implemented");
    int r;
    envid_t ceid;

    set_pgfault_handler(pgfault);
    
    if (ceid = sys_exofork(), ceid == 0) {
        thisenv = &envs[ENVX(sys_getenvid())];
    } else if (ceid > 0) {
        // assume UTOP == UXSTACKTOP
        for (size_t pn = 0; pn < UTOP / PGSIZE - 1;) {
            uint32_t pde = uvpd[pn / NPDENTRIES];
            if (!(pde & PTE_P)) {
                pn += NPDENTRIES;
            } else {
                size_t next = MIN(UTOP / PGSIZE - 1, pn + NPDENTRIES);
                for (; pn < next; ++pn) {
                    uint32_t pte = uvpt[pn];
                    if (pte & PTE_P && pte & PTE_U)
                        if (r = duppage(ceid, pn), r < 0)
                            PANIC;
                }
            }
        }
        if (r = sys_page_alloc(ceid, (void*)(UXSTACKTOP - PGSIZE), 
                               PTE_P | PTE_U | PTE_W), r < 0)
            PANIC;
        if (r = sys_env_set_pgfault_upcall(ceid, _pgfault_upcall), r < 0)
            PANIC;
        if (r = sys_env_set_status(ceid, ENV_RUNNABLE), r < 0)
            PANIC;
    }
    return ceid;

#undef PANIC
}
```



### `duppage`

按要求拷贝页面即可：

```C
static int
duppage(envid_t envid, unsigned pn)
{
	// LAB 4: Your code here.
	// panic("duppage not implemented");
    int r;
    void *pg;
    pte_t pte;

    pg = (void*)(pn * PGSIZE);
    pte = uvpt[pn];

    assert(pte & PTE_P && pte & PTE_U);
    if (pte & PTE_W || pte & PTE_COW) {
        if (r = sys_page_map(0, pg, envid, pg, PTE_P | PTE_U | PTE_COW), r < 0)
            return r;
        if (r = sys_page_map(0, pg, 0, pg, PTE_P | PTE_U | PTE_COW), r < 0)
            return r;
    } else {
        if (r = sys_page_map(0, pg, envid, pg, PTE_P | PTE_U), r < 0)
            return r;
    }

	return 0;
}
```



### `pgfault`

过程比较简单，主要是检查出错地址比较繁琐：

```C
static void
pgfault(struct UTrapframe *utf)
{
#define PANIC panic("page fault handler: %e", r)

	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    addr = ROUNDDOWN(addr, PGSIZE);

    if (!(
            (uvpd[PDX(addr)] & PTE_P) && 
            (uvpt[PGNUM(addr)] & PTE_P) && 
            (uvpt[PGNUM(addr)] & PTE_U) && 
            (uvpt[PGNUM(addr)] & PTE_COW) && 
            (err & FEC_WR) && 
            1
        )) {
        panic(
            "[0x%08x] user page fault va 0x%08x ip 0x%08x: "
            "[%s, %s, %s]",
            sys_getenvid(),
            utf->utf_fault_va,
            utf->utf_eip,
            err & 4 ? "user" : "kernel",
            err & 2 ? "write" : "read",
            err & 1 ? "protection" : "not-present"
        );
    }

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    if (r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W), r < 0)
        PANIC;
    memmove(PFTEMP, addr, PGSIZE);
    if (r = sys_page_map(0, PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W), r < 0)
        PANIC;
    if (r = sys_page_unmap(0, PFTEMP), r < 0)
        PANIC;
    return;

#undef PANIC
}
```



## Exercise 13

对我来说只需要在`kern/trapentry.inc`里添加几项即可：

```C
TH(32) // TIMER
TH(33) // KBD
TH(36) // SERIAL
TH(39) // SPURIOUS
TH(46) // IDE
TH(51) // ERROR
```



在`kern/env.c`的`env_alloc`里加入：

```C
	// Enable interrupts while in user mode.
	// LAB 4: Your code here.
    e->env_tf.tf_eflags |= FL_IF;
```



## Exercise 14

在`trap_dispatch`中加入：

```C
    if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
		lapic_eoi();
		sched_yield();
		return;
    }
```



## Exercise 15

### `kern/syscall.c`

定义了`ipc_try_send`和`ipc_send_page`作为辅助函数，其他实现按要求即可：

```C
static int 
ipc_send_page(struct Env* srce, struct Env* dste)
{
    int r;
    void * srcva = srce->env_ipc_dstva;
    void * dstva = dste->env_ipc_dstva;
    unsigned perm = srce->env_ipc_perm;
    if (srcva < (void*)UTOP && dstva < (void*)UTOP) {
        struct PageInfo *pp;
        pte_t *pte;
        if ((uint32_t)srcva & (PGSIZE - 1))
            return -E_INVAL;
        if (!(perm & PTE_P) || !(perm & PTE_U) || perm & ~PTE_SYSCALL)
            return -E_INVAL;
        if (pp = page_lookup(srce->env_pgdir, srcva, &pte), !pp)
            return -E_INVAL;
        if (perm & PTE_W && !(*pte & PTE_W))
            return -E_INVAL;
        if (r = page_insert(dste->env_pgdir, pp, dstva, perm), r < 0)
            return r;
    } else {
        dste->env_ipc_perm = 0;
    }
    return 0;
}

static int 
ipc_try_send(struct Env* dste, uint32_t value, void* srcva, unsigned perm)
{
    int r;
    struct Env *cure = curenv;

    if (!(dste->env_ipc_recving && dste->env_status == ENV_NOT_RUNNABLE))
        return -E_IPC_NOT_RECV;

    // map page
    cure->env_ipc_dstva = srcva;
    cure->env_ipc_perm = perm;
    if (r = ipc_send_page(cure, dste), r < 0) 
        return r;

    // ok
    dste->env_ipc_recving = 0;
    dste->env_ipc_from = cure->env_id;
    dste->env_ipc_value = value;
    dste->env_status = ENV_RUNNABLE;
    dste->env_tf.tf_regs.reg_eax = 0;
    
    return 0;
}

static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	// panic("sys_ipc_try_send not implemented");
    int r;
    struct Env *dste;

    if (r = envid2env(envid, &dste, 0), r < 0)
        return r;
    return ipc_try_send(dste, value, srcva, perm);
}

static int
sys_ipc_recv(void *dstva)
{
    int r;
    struct Env *cure = curenv;
	// LAB 4: Your code here.
	// panic("sys_ipc_recv not implemented");
    if (dstva < (void*)UTOP && (uint32_t)dstva & (PGSIZE - 1))
        return -E_INVAL;
    cure->env_ipc_dstva = dstva;
    cure->env_ipc_recving = 1;
    cure->env_status = ENV_NOT_RUNNABLE;
    sched_yield();
	return 0;
}
```



### `lib/ipc.c`

按要求实现即可：

```C
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	// LAB 4: Your code here.
	// panic("ipc_recv not implemented");
    int r;
    envid_t feid;
    int perm;
    
    if (r = sys_ipc_recv(pg ? ROUNDDOWN(pg, PGSIZE) : (void*)UTOP), r < 0) {
        feid = 0;
        perm = 0;
    } else {
        feid = thisenv->env_ipc_from;
        perm = thisenv->env_ipc_perm;
    }
    if (from_env_store)
        *from_env_store = feid;
    if (perm_store)
        *perm_store = perm;
    return r < 0 ? r : thisenv->env_ipc_value;
}

void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
	// panic("ipc_send not implemented");
    int r;
    
    while(r = sys_ipc_try_send(to_env, val, pg ? 
                               ROUNDDOWN(pg, PGSIZE) : 
                               (void*)UTOP, perm), r == -E_IPC_NOT_RECV)
        sys_yield();
    if (r < 0)
        panic("ipc send: %e", r);
}
```





## This completes this lab

```
dumbfork: OK (0.9s) 
Part A score: 5/5

faultread: OK (0.8s) 
faultwrite: OK (1.2s) 
faultdie: OK (1.6s) 
faultregs: OK (2.2s) 
faultalloc: OK (1.0s) 
faultallocbad: OK (1.7s) 
faultnostack: OK (2.2s) 
faultbadhandler: OK (2.1s) 
faultevilhandler: OK (1.9s) 
forktree: OK (1.9s) 
Part B score: 50/50

spin: OK (2.1s) 
stresssched: OK (2.3s) 
sendpage: OK (1.8s) 
pingpong: OK (1.8s) 
primes: OK (3.2s) 
Part C score: 25/25

Score: 80/80
```



注：以下结果为开启了`CONF_IPC_SLEEP`和`CONF_MFQ`后的测试结果，故这两个相关的challenge的测试结果之后就不再赘述。从中可以看出部分测试的运行时间改进明显（如`stresssched`）。

```
dumbfork: OK (1.3s) 
Part A score: 5/5

faultread: OK (0.9s) 
faultwrite: OK (1.1s) 
faultdie: OK (1.7s)
faultregs: OK (1.5s) 
faultalloc: OK (1.4s) 
faultallocbad: OK (1.4s) 
faultnostack: OK (1.7s) 
faultbadhandler: OK (2.0s) 
faultevilhandler: OK (2.3s) 
forktree: OK (1.6s) 
Part B score: 50/50

spin: OK (2.9s) 
stresssched: OK (1.6s) 
sendpage: OK (1.7s) 
pingpong: OK (2.1s) 
primes: OK (3.0s) 
Part C score: 25/25

Score: 80/80
```



## Challenge: Sleep IPC

### 描述

实现了一个新的系统调用`sys_ipc_send`，若目标进程未处于等待状态，会阻塞。接口与`sys_ipc_try_send`一致。



### 原理

+ 若调用`sys_ipc_send`时目标进程未处于接收状态，则加入目标进程的IPC阻塞队列，并将自己的状态设为阻塞；否则正常发送。
+ 若调用`sys_ipc_recv`时发现自己的IPC阻塞队列非空，则直接取队首的进程接收；否则阻塞。



### 实现

#### `inc/config.h`

在config文件（Lab2时自己添加的，用于启动或者关闭一些可选功能）中添加选项：

```c
#define CONF_IPC_SLEEP
```



#### `inc/elink.h`

创建了一个新的头文件，定义了嵌于其他结构体中的通用链表节点`EmbedLink`以及相关的操作函数（参考自Linux的一些数据结构）：

```C

#include <inc/types.h>

typedef struct EmbedLink {
    struct EmbedLink *prev, *next;
} EmbedLink;

static inline void 
elink_init(EmbedLink *ln) 
{
    ln->prev = ln->next = ln;
}

static inline EmbedLink* 
elink_remove(EmbedLink *ln) 
{
    ln->prev->next = ln->next;
    ln->next->prev = ln->prev;
    elink_init(ln);
    return ln;
}

static inline void 
elink_insert(EmbedLink *pos, EmbedLink *ln) 
{
    ln->prev = pos, ln->next = pos->next;
    ln->prev->next = ln->next->prev = ln;
}

static inline bool 
elink_empty(EmbedLink * ln) 
{
    if (ln->prev == ln) {
        assert(ln->next == ln);
        return true;
    } else {
        return false;
    }
}

static inline void 
elink_enqueue(EmbedLink *que, EmbedLink *ln) 
{
    elink_insert(que->prev, ln);
}

static inline EmbedLink*
elink_queue_head(EmbedLink* que)
{
    return que->next;
}

static inline EmbedLink* 
elink_dequeue(EmbedLink* que)
{
    return elink_remove(elink_queue_head(que));
}


#define offset(_t, _m) ((uint32_t)(&((_t*)0)->_m))
#define master(_x, _t, _m) ((_t*)((void*)(_x) - offset(_t, _m)))
```



#### `inc/env.h`

在`struct Env`中添加两个成员：

```C
    EmbedLink env_ipc_link;         // Embeded link to the blocking queue.
    EmbedLink env_ipc_queue;        // Blocking queue.
```



#### `kern/env.c`

在`env_alloc`中初始化新增成员：

```C
	elink_init(&e->env_ipc_link);
	elink_init(&e->env_ipc_queue);
```

在`env_free`中唤醒阻塞的IPC发送者，并将自己从IPC阻塞队列中取下：

```C
	// wakeup the sleeping senders
	while(!elink_empty(&e->env_ipc_queue)) {
		struct Env* sender = master(elink_dequeue(&e->env_ipc_queue), 
                                    struct Env, env_ipc_link);
		sender->env_status = ENV_RUNNABLE;
		sender->env_tf.tf_regs.reg_eax = -E_BAD_ENV;
	}
	elink_remove(&e->env_ipc_link);
```



#### `kern/syscall.c`

添加`sys_ipc_send`，并修改`sys_ipc_recv`：

```C
static int 
sys_ipc_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
    int r;
    struct Env* dste;
    struct Env* cure = curenv;

    if (r = envid2env(envid, &dste, 0), r < 0)
        return r;
    if (r = ipc_try_send(dste, value, srcva, perm), r != -E_IPC_NOT_RECV)
        return r;

    cure->env_ipc_dstva = srcva;
    cure->env_ipc_perm = perm;
    cure->env_ipc_value = value;
    cure->env_status = ENV_NOT_RUNNABLE;

    elink_enqueue(&dste->env_ipc_queue, &cure->env_ipc_link);
    sched_yield();
    return 0;
}

static int
sys_ipc_recv(void *dstva)
{
    int r;
    struct Env *cure = curenv;
	// LAB 4: Your code here.
	// panic("sys_ipc_recv not implemented");
    if (dstva < (void*)UTOP && (uint32_t)dstva & (PGSIZE - 1))
        return -E_INVAL;

    cure->env_ipc_dstva = dstva;
    if (!elink_empty(&cure->env_ipc_queue)) {
        EmbedLink* ln = elink_dequeue(&cure->env_ipc_queue);
        struct Env* sender = master(ln, struct Env, env_ipc_link);

        r = ipc_send_page(sender, cure);
            
        sender->env_status = ENV_RUNNABLE;
        sender->env_tf.tf_regs.reg_eax = r;

        if (r < 0)
            goto sleep;

        cure->env_ipc_from = sender->env_id;
        cure->env_ipc_value = sender->env_ipc_value;
        return 0;
    }

sleep:
    cure->env_ipc_recving = 1;
    cure->env_status = ENV_NOT_RUNNABLE;
    sched_yield();
	return 0;
}
```



#### `lib/ipc.h`

修改`ipc_send`函数：

```C
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
	// panic("ipc_send not implemented");
    int r;

#ifndef CONF_IPC_SLEEP
    while(r = sys_ipc_try_send(to_env, val, pg ? 
                               ROUNDDOWN(pg, PGSIZE) : 
                               (void*)UTOP, perm), r == -E_IPC_NOT_RECV)
        sys_yield();
#else
    r = sys_ipc_send(to_env, val, pg ? ROUNDDOWN(pg, PGSIZE) : (void*)UTOP, perm);
#endif

    if (r < 0)
        panic("ipc send: %e", r);
}
```



## Challenge: Multilevel Feedback Queue

实现了多级反馈队列（MFQ）调度算法。

原理不再赘述，下面仅说明实现：

### 实现

#### `inc/config.h`

新增选项：

```C
#define CONF_MFQ
```



#### `inc/env.h`

在`struct Env`中添加以下成员，用于记录必要信息（取名比较直白，就不注释说明了）。

```C
#ifdef CONF_MFQ
    EmbedLink env_mfq_link;
    uint32_t env_mfq_level;
    int env_mfq_left_ticks;
#endif
```



#### `kern/env.h`

添加以下内容。其中`env_ready(e)`可用于代替所有的`e->env_status = ENV_RUNNABLE`语句，修改的地方不再赘述：

```C
#ifdef CONF_MFQ 

#define NMFQ 5
#define MFQ_SLICE 2
extern EmbedLink* mfqs;

void 	env_mfq_add(struct Env* e);
void 	env_mfq_pop(struct Env* e);

#endif

void 	env_ready(struct Env* e);
```



#### `kern/pmap.c`

在`mem_init`初始化`mfq`：

```C
#ifdef CONF_MFQ
	//////////////////////////////////////////////////////////////////////
	// Make 'mfqs' point to an array of size 'NMFQ' of 'EmbedLink'.
	mfqs = (EmbedLink* ) boot_alloc(NMFQ * sizeof(EmbedLink));
	memset(mfqs, 0, NMFQ * sizeof(EmbedLink));
	for (int i = 0 ; i < NMFQ; ++i)
		elink_init(&mfqs[i]);
#endif
```



#### `kern/env.c`

实现MFQ相关函数。

其中`env_mfq_add`用于将进程加入队列，通常通过`env_ready`调用。

`env_mfq_pop`将进程从队列中移除。

```C

#ifdef CONF_MFQ
EmbedLink* mfqs = NULL;

void 
env_mfq_add(struct Env *e) 
{
	elink_remove(&e->env_mfq_link);
	if (e->env_mfq_left_ticks > 0) { // time slice left
         // add to queue's head
		elink_insert(&mfqs[e->env_mfq_level], &e->env_mfq_link);	
	} else { // no time slice left
		uint32_t lv = MIN(e->env_mfq_level + 1, NMFQ - 1);
		e->env_mfq_level = lv;
		e->env_mfq_left_ticks = (1 << lv) * MFQ_SLICE;
		elink_enqueue(&mfqs[lv], &e->env_mfq_link);
	}
}

void 
env_mfq_pop(struct Env* e)
{
	elink_remove(&e->env_mfq_link);
}

#endif

void 
env_ready(struct Env* e)
{
	e->env_status = ENV_RUNNABLE;
#ifdef CONF_MFQ
	env_mfq_add(e);
#endif
}
```

在`env_alloc`中初始化相关成员：

```C
#ifdef CONF_MFQ
	e->env_mfq_level = 0;
	e->env_mfq_left_ticks = MFQ_SLICE;
	elink_enqueue(&mfqs[0], &e->env_mfq_link);
#endif
```

在`env_free`中处理相关成员：

```C
#ifdef CONF_MFQ
	e->env_mfq_level = 0;
	e->env_mfq_left_ticks = 0;
	env_mfq_pop(e);
#endif
```

修改`env_run`，将当前要运行的进程从队列中移除。注意，只要进程不处于就绪态就要从MFQ中移除，包括IPC发送、接收时的阻塞，这些方面的修改就不再赘述了。

```C
void
env_run(struct Env *e)
{
	struct Env* olde = curenv;

    if (olde && olde->env_status == ENV_RUNNING)
        env_ready(olde);
    curenv = e;
    e->env_status = ENV_RUNNING;
    e->env_runs += 1;

#ifdef CONF_MFQ
	env_mfq_pop(e);
#endif

	if (olde != e) // 只有进程切换才进行，尽可能避免TLB失效
		lcr3(PADDR(e->env_pgdir));
    unlock_kernel();
    env_pop_tf(&e->env_tf);
}
```



#### `kern/sched.c`

修改`sched_yield`函数，实现MFQ调度算法：

```C

#ifndef CONF_MFQ
// RR scheduler
#else 
void 
sched_yield(void)
{
	struct Env* idle = NULL;

	for (int i = 0; i < NMFQ; ++i) {
		if (!elink_empty(&mfqs[i])) {
			idle = master(elink_queue_head(&mfqs[i]), struct Env, env_mfq_link);
			// assert(idle->env_status == ENV_RUNNABLE);
			if (idle->env_status != ENV_RUNNABLE) {
				log_int(idle->env_status);
				panic("only runnable env can be in mfq");	
			}
			break;
		}
	}
	if (idle) {
		env_run(idle);
	} else if (curenv && curenv->env_status == ENV_RUNNING) {
		env_run(curenv);
	}
	sched_halt();
}
#endif
```



#### `kern/trap.c`

在`trap_dispatch`中修改对时钟中断的处理：

```C
    if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
#ifndef CONF_MFQ
		lapic_eoi();
		sched_yield();
		return;
#else
		struct Env* cure = curenv;
		lapic_eoi();
		if (cure && cure->env_mfq_left_ticks-- == 1) {
			sched_yield();
		}
        return;
#endif
    }
```



## Challenge: Process snapshot and rollback

### 描述

实现了两个系统调用`sys_snapshot`和`sys_rollback`。

接口如下：

```C
envid_t	sys_env_snapshot(uint32_t *dmail_store);
int sys_env_rollback(envid_t spst, uint32_t dmail);
```

`sys_env_snapshot`：

+ 参数`dmail_store`：用于存储来自“未来”的信息。
+ 返回envid：作为snapshot的标识。
+ 出错返回负数。

`sys_env_rollback`：

+ 参数`spst`：回滚的目标snapshot。
+ 参数`dmail`：传递给“过去”的信息。
+ 通常不返回，出错则返回负数。



以下为一个使用示例：

```C
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
```

输出：

```
[00000000] new env 00001000
[00001000] new env 00001001
recv dmail 0 at time(0:0)
recv dmail 1 at time(0:0)
recv dmail 2 at time(0:0)
recv dmail 3 at time(0:0)
recv dmail 4 at time(0:0)
recv dmail 5 at time(0:0)
recv dmail 6 at time(0:0)
recv dmail 7 at time(0:0)
recv dmail 8 at time(0:0)
recv dmail 9 at time(0:0)
[00001000] exiting gracefully
[00001000] free env 00001000
[00001000] free env 00001001
No runnable environments in the system!
```



### 原理

基本原理和fork类似，不过核心部分均在内核而不是用户态实现（主要是出于效率方面的考虑）：

+ snapshot的时候分配一个新的快照进程，加入当前进程的快照列表中（每个进程可以持有多个快照，按时间排列）；用类似fork的方式复制当前进程的页到快照进程。
+ rollback的时候，先将快照列表中目标快照之后的快照释放（符合直觉，而且可以防止不断回滚不断重复做快照让PCB耗尽）。之后用目标快照恢复当前进程的寄存器、内存等信息。



### 实现

### `inc/env.h`

新增一个`EnvType`：`ENV_TYPE_SPST`，用于表示作为快照的进程

```C
// Special environment types
enum EnvType {
	ENV_TYPE_USER = 0,
    ENV_TYPE_SPST,  // snapshot
};
```

在`struct Env`中新增以下成员：

```c
    EmbedLink env_spst_link;    // Embeded link to the snapshot list
    envid_t env_spst_owner_id;  // snapshot's owner env's id
    envid_t env_spst_id;        // rollbacked snapshot's id
    uint32_t env_spst_dmail;    // DeLorean-Mail（这一切都是命运石之门的选择!）
#define EMPTY_DMAIL ~0U
```



### `kern/syscall.c`

实现`sys_env_snapshot`和`sys_env_rollback`：

```C
static envid_t
sys_env_snapshot(void)
{
#define CHECK(x) do {if (r = (x), r < 0) return r;} while(0)

    int r;
    struct Env* cure = curenv;
    struct Env* e;
    
    CHECK(env_alloc(&e, cure->env_id));

    e->env_tf = cure->env_tf;
    e->env_status = ENV_NOT_RUNNABLE;
    e->env_type = ENV_TYPE_SPST;
    e->env_spst_owner_id = cure->env_id;
    elink_enqueue(&cure->env_spst_link, &e->env_spst_link);

#ifdef CONF_MFQ
    env_mfq_pop(e);
#endif

    for (uint32_t pdeno = 0; pdeno < PDX(UTOP); ++pdeno) {
        if ((cure->env_pgdir[pdeno] & PTE_P) == 0)
            continue;
        pte_t* pt = (pte_t*)KADDR(PTE_ADDR(cure->env_pgdir[pdeno]));

        for (uint32_t pteno = 0; pteno < NPDENTRIES; ++pteno) {
            if (pt[pteno] & PTE_P) {
                pte_t pte = pt[pteno];
                struct PageInfo* pp = pa2page(PTE_ADDR(pte));
                void* va = PGADDR(pdeno, pteno, 0);
                assert(pte & PTE_U);
                if (pte & PTE_W || pte & PTE_COW) {
                    CHECK(page_insert(e->env_pgdir, pp, va, PTE_P | PTE_U | PTE_COW));
                    CHECK(page_insert(cure->env_pgdir, pp, va, PTE_P | PTE_U | PTE_COW));
                } else {
                    CHECK(page_insert(e->env_pgdir, pp, va, PTE_P | PTE_U));
                }
            }
        }
    }
    struct PageInfo* pp = page_lookup(cure->env_pgdir, (void*)(UXSTACKTOP - PGSIZE), 0);
    CHECK(page_insert(e->env_pgdir, pp, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W));
    CHECK(page_insert(cure->env_pgdir, pp, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W));

    cure->env_spst_dmail = EMPTY_DMAIL;

    return e->env_id;

#undef CHECK
}
```

```C
static int 
sys_env_rollback(envid_t eid, uint32_t dmail)
{
#define CHECK(x) do {if (r = (x), r < 0) return r;} while(0)

    int r;
    struct Env* cure = curenv;
    struct Env* e;
    envid_t ceid;

    CHECK(envid2env(eid, &e, 1));
    if (e->env_type != ENV_TYPE_SPST || e->env_spst_owner_id != cure->env_id)
        return -E_INVAL;

    // 将该快照之后记录的快照全部删除
    for(EmbedLink* ln = &e->env_spst_link; ln->next != &cure->env_spst_link; ) {
        struct Env* tmpe = master(elink_remove(ln->next), struct Env, env_spst_link);
        env_free(tmpe);
    }

    // 将快照进程的页映射到当前进程，并将当前进程多余的页删去
    for (uint32_t pdeno = 0; pdeno < PDX(UTOP); ++pdeno) {
        if (e->env_pgdir[pdeno] & PTE_P) {
            pte_t *pt = (pte_t *)KADDR(PTE_ADDR(e->env_pgdir[pdeno]));
            pte_t *pt2 = (cure->env_pgdir[pdeno] & PTE_P)
                             ? (pte_t *)KADDR(PTE_ADDR(cure->env_pgdir[pdeno]))
                             : NULL;
            for (uint32_t pteno = 0; pteno < NPDENTRIES; ++pteno) {
                if (pt[pteno] & PTE_P) {
                    pte_t pte = pt[pteno];
                    CHECK(page_insert(cure->env_pgdir, pa2page(PTE_ADDR(pte)),
                                PGADDR(pdeno, pteno, 0), pte & PTE_SYSCALL));
                } else if (pt2 && pt2[pteno] & PTE_P) {
                    page_remove(cure->env_pgdir, PGADDR(pdeno, pteno, 0));
                }
            }
        } else if (cure->env_pgdir[pdeno] & PTE_P) {
            pte_t *pt = (pte_t*)KADDR(PTE_ADDR(cure->env_pgdir[pdeno]));
            for (uint32_t pteno = 0; pteno < NPDENTRIES; ++pteno) {
                if (pt[pteno] & PTE_P)
                    page_remove(cure->env_pgdir, PGADDR(pdeno, pteno, 0));
            }
        }
    }

    cure->env_tf = e->env_tf;
    cure->env_spst_dmail = dmail;

    return e->env_id;

#undef CHECK
}
```



### `lib/fork.c`

实现`sys_env_snapshot`用户态的接口。之所以在`lib/fork.c`而不是`lib/syscall.c`里实现是为了设置page fault handler为`pgfault`。

```C
static inline int32_t
syscall(int num, int check, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	int32_t ret;

	asm volatile("int %1\n"
		     : "=a" (ret)
		     : "i" (T_SYSCALL),
		       "a" (num),
		       "d" (a1),
		       "c" (a2),
		       "b" (a3),
		       "D" (a4),
		       "S" (a5)
		     : "cc", "memory");

	if(check && ret > 0)
		panic("syscall %d returned %d (> 0)", num, ret);

	return ret;
}

envid_t
sys_env_snapshot(uint32_t *dmail_store)
{
 #define PANIC panic("sys_env_snapshot: %e", r)

    int r;
    set_pgfault_handler(pgfault);

    if (r = syscall(SYS_env_snapshot, 0, 0, 0, 0, 0, 0), r < 0)
        PANIC;
    if (dmail_store)
        *dmail_store = thisenv->env_spst_dmail;
    return r;

#undef PANIC   
}
```



### `lib/syscall.c`

实现`sys_env_rollback`的用户态接口：

```C
int 
sys_env_rollback(envid_t eid, uint32_t dmail)
{
	if (dmail == EMPTY_DMAIL)
		return -E_INVAL;
	int r = syscall(SYS_env_rollback, 1, (uint32_t)eid, dmail, 0, 0, 0);
	if (r == 0)
		panic("rollback should never return");
	return r;
}
```



## Compatible to Page Size Extension

在本Lab的多处理器环境下若要兼容Lab2一个Challenge中的大页扩展，需要在`kern/init.c`的`mp_main`函数内，加载页表之前设置好CR4寄存器的值：

```C
void
mp_main(void)
{
#ifdef CONF_HUGE_PAGE
	// Enable page size extension
	uint32_t cr4;
	cr4 = rcr4();
	cr4 |= CR4_PSE;
	lcr4(cr4);
#endif
	// We are in high EIP now, safe to switch to kern_pgdir 
	lcr3(PADDR(kern_pgdir));
	// ......
}
```

