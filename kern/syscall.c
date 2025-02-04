/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/log.h>
#include <inc/elink.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
    user_mem_assert(curenv, s, len, PTE_U);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	// panic("sys_exofork not implemented");
    int r;
    struct Env *parent, *child;
    
    parent = curenv;
    if (r= env_alloc(&child, parent->env_id), r < 0)
        return r;

    child->env_status = ENV_NOT_RUNNABLE;
    child->env_tf = parent->env_tf;
    child->env_tf.tf_regs.reg_eax = 0;

#ifdef CONF_MFQ
    env_mfq_pop(child);
#endif

    return child->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	// panic("sys_env_set_status not implemented");
    int r;
    struct Env *e;

    if (status < 0 || status > ENV_NOT_RUNNABLE)
        return -E_INVAL;
    if (r = envid2env(envid, &e, 1), r < 0)
        return r;
    if (e->env_type == ENV_TYPE_SPST)
        return -E_INVAL;

#ifdef CONF_MFQ
    if (status == ENV_RUNNABLE)
        env_mfq_add(e);
#endif

    e->env_status = status;
    return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3), interrupts enabled, and IOPL of 0.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	// panic("sys_env_set_trapframe not implemented");
    int r;
    struct Env *e;

    if (r = envid2env(envid, &e, 1), r < 0)
        return r;

    e->env_tf = *tf;
    tf = &e->env_tf;

    tf->tf_eflags |= FL_IF;
    tf->tf_eflags &= ~FL_IOPL_MASK;
    tf->tf_cs |= GD_UT | 3;
    tf->tf_ds |= GD_UD | 3;
    tf->tf_es |= GD_UD | 3;
    tf->tf_ss |= GD_UD | 3;

    return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	// panic("sys_env_set_pgfault_upcall not implemented");
    int r;
    struct Env *e;
    
    if (r = envid2env(envid, &e, 1), r < 0)
        return r;
    e->env_pgfault_upcall = func;
    return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!
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
	// LAB 4: Your code here.
	// panic("sys_page_alloc not implemented");
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

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them. ???
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	// panic("sys_page_map not implemented");
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

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	// panic("sys_page_unmap not implemented");
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


// // Try to send 'value' to the target env 'envid'.
// // If srcva < UTOP, then also send page currently mapped ast 'srcva',
// // so that receiver gets a duplicate mapping of the same page.
// //
// // The send fails with a return value of -E_IPC_NOT_RECV if the
// // target is not blocked, waiting for an IPC.
// //
// // The send also can fail for the other reasons listed below.
// //
// // Otherwise, the send succeeds, and the target's ipc fields are
// // updated as follows:
// //    env_ipc_recving is set to 0 to block future sends;
// //    env_ipc_from is set to the sending envid;
// //    env_ipc_value is set to the 'value' parameter;
// //    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// // The target environment is marked runnable again, returning 0
// // from the paused sys_ipc_recv system call.  (Hint: does the
// // sys_ipc_recv function ever actually return?)
// //
// // If the sender wants to send a page but the receiver isn't asking for one,
// // then no page mapping is transferred, but no error occurs.
// // The ipc only happens when no errors occur.
// //
// // Returns 0 on success, < 0 on error.
// // Errors are:
// //	-E_BAD_ENV if environment envid doesn't currently exist.
// //		(No need to check permissions.)
// //	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
// //		or another environment managed to send first.
// //	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
// //	-E_INVAL if srcva < UTOP and perm is inappropriate
// //		(see sys_page_alloc).
// //	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
// //		address space.
// //	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
// //		current environment's address space.
// //	-E_NO_MEM if there's not enough memory to map srcva in envid's
// //		address space.
// static int
// sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
// {
// 	// LAB 4: Your code here.
// 	// panic("sys_ipc_try_send not implemented");
//     int r;
//     struct Env *dste;

//     if (r = envid2env(envid, &dste, 0), r < 0)
//         return r;
//     if (!dste->env_ipc_recving || dste->env_status != ENV_NOT_RUNNABLE) {
//         // log("ENV %08x: Well, no respond...\n", curenv->env_id);
//         return -E_IPC_NOT_RECV;
//     }
//     // map page
//     if (srcva < (void*)UTOP && dste->env_ipc_dstva < (void*)UTOP) {
//         struct PageInfo *pp;
//         pte_t *pte;
//         if ((uint32_t)srcva & (PGSIZE - 1))
//             return -E_INVAL;
//         if (!(perm & PTE_P) || !(perm & PTE_U) || perm & ~PTE_SYSCALL)
//             return -E_INVAL;
//         if (pp = page_lookup(curenv->env_pgdir, srcva, &pte), !pp)
//             return -E_INVAL;
//         if (perm & PTE_W && !(*pte & PTE_W))
//             return -E_INVAL;
//         if (r = page_insert(dste->env_pgdir, pp, dste->env_ipc_dstva, perm), r < 0)
//             return r;
//         dste->env_ipc_perm = perm;
//     } else {
//         dste->env_ipc_perm = 0;
//     }
//     // ok
//     dste->env_ipc_recving = 0;
//     dste->env_ipc_from = curenv->env_id;
//     dste->env_ipc_value = value;
//     dste->env_status = ENV_RUNNABLE;
//     dste->env_tf.tf_regs.reg_eax = 0;
//     // log("ENV %08x: She just received it!\n", curenv->env_id);
//     return 0;
// }

// // Block until a value is ready.  Record that you want to receive
// // using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// // mark yourself not runnable, and then give up the CPU.
// //
// // If 'dstva' is < UTOP, then you are willing to receive a page of data.
// // 'dstva' is the virtual address at which the sent page should be mapped.
// //
// // This function only returns on error, but the system call will eventually
// // return 0 on success.
// // Return < 0 on error.  Errors are:
// //	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
// static int
// sys_ipc_recv(void *dstva)
// {
// 	// LAB 4: Your code here.
// 	// panic("sys_ipc_recv not implemented");
//     if (dstva < (void*)UTOP && (uint32_t)dstva & (PGSIZE - 1))
//         return -E_INVAL;
//     struct Env *e = curenv;
//     e->env_ipc_dstva = dstva;
//     e->env_ipc_recving = 1;
//     e->env_status = ENV_NOT_RUNNABLE;
//     sched_yield();
// 	return 0;
// }


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
        dste->env_ipc_perm = srce->env_ipc_perm;
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
    // dste->env_status = ENV_RUNNABLE;
    dste->env_tf.tf_regs.reg_eax = 0;
    env_ready(dste);

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
    if (dstva < (void*)UTOP && (uint32_t)dstva & (PGSIZE - 1))
        return -E_INVAL;

    cure->env_ipc_dstva = dstva;
    if (!elink_empty(&cure->env_ipc_queue)) {
        EmbedLink* ln = elink_dequeue(&cure->env_ipc_queue);
        struct Env* sender = master(ln, struct Env, env_ipc_link);

        r = ipc_send_page(sender, cure);
            
        // sender->env_status = ENV_RUNNABLE;
        sender->env_tf.tf_regs.reg_eax = r;
        env_ready(sender);

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

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	// panic("sys_time_msec not implemented");
    return time_msec();
}

static int 
sys_dl_transmit(const char* buf, size_t len) {
    user_mem_assert(curenv, buf, len, PTE_U);
    return e1000_transmit(buf, len);    
}

static int 
sys_dl_receive(char *buf, size_t len) 
{
    user_mem_assert(curenv, buf, len, PTE_U | PTE_W);
    return e1000_receive(buf, len);
}

static int 
sys_dl_read_mac_addr(uint8_t *mac)
{
    user_mem_assert(curenv, mac, 6, PTE_U | PTE_W);
    uint64_t mac_addr = e1000_read_mac_addr();
    for (int i = 0; i < 6; ++i)
        mac[i] = (uint8_t)(mac_addr >> (8 * i));
    return 0;
}

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

// Dispatches to the correct kernel function, passing the arguments.
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
    case SYS_yield: {
        sys_yield();
        return 0;
    }
    case SYS_exofork: {
        return sys_exofork();
    }
    case SYS_env_set_status: {
        return sys_env_set_status((envid_t)a1, (int)a2);
    }
    case SYS_env_set_trapframe: {
        return sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2);
    }
    case SYS_env_set_pgfault_upcall: {
        return sys_env_set_pgfault_upcall((envid_t)a1, (void*)a2);
    }
    case SYS_page_alloc: {
        return sys_page_alloc((envid_t)a1, (void*)a2, (int)a3);
    }
    case SYS_page_map: {
        return sys_page_map((envid_t)a1, (void*)a2, (envid_t)a3, (void*)a4, (int)a5);
    }
    case SYS_page_unmap: {
        return sys_page_unmap((envid_t)a1, (void*)a2);
    }
    case SYS_ipc_send: {
        return sys_ipc_send((envid_t)a1, (uint32_t)a2, (void*)a3, (unsigned int)a4);
    }
    case SYS_ipc_try_send: {
        return sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void*)a3, (unsigned int)a4);
    } 
    case SYS_ipc_recv: {
        return sys_ipc_recv((void*)a1);
    }
    case SYS_time_msec: {
        return sys_time_msec();
    }
    case SYS_dl_transmit: {
        return sys_dl_transmit((const char *)a1, (size_t)a2);
    }
    case SYS_dl_receive: {
        return sys_dl_receive((char *)a1, (size_t)a2);
    }
    case SYS_dl_read_mac_addr: {
        return sys_dl_read_mac_addr((uint8_t *)a1);
    }
    case SYS_env_snapshot: {
        return sys_env_snapshot();
    }
    case SYS_env_rollback: {
        return sys_env_rollback((envid_t)a1, (uint32_t)a2);
    }
	default:
		return -E_INVAL;
	}
}

