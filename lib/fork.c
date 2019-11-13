// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/log.h>
#include <inc/assert.h>

extern volatile pde_t uvpd[];
extern volatile pte_t uvpt[];
extern void _pgfault_upcall(void);


//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
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

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
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

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
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
        if (r = sys_page_alloc(ceid, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W), r < 0)
            PANIC;
        if (r = sys_env_set_pgfault_upcall(ceid, _pgfault_upcall), r < 0)
            PANIC;
        if (r = sys_env_set_status(ceid, ENV_RUNNABLE), r < 0)
            PANIC;
    }
    return ceid;

#undef PANIC
}

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

static int 
sduppage(envid_t envid, unsigned pn) 
{
    int r;
    void *pg;
    pte_t pte;

    pg = (void*)(pn * PGSIZE);
    pte = uvpt[pn];

    assert(pte & PTE_P && pte & PTE_U);
    if (r = sys_page_map(0, pg, envid, pg, pte & PTE_SYSCALL), r < 0)
        return r;

    return 0;   
}

// Challenge!
int
sfork(void)
{
	// panic("sfork not implemented");
#define PANIC panic("sfork: %e", r)
    int r;
    envid_t ceid, peid;

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
                        if (r = sduppage(ceid, pn), r < 0)
                            PANIC;
                }
            }
        }
        if (r = duppage(ceid, (USTACKTOP - 1) / PGSIZE), r < 0)
            PANIC;
        if (r = sys_page_alloc(ceid, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W), r < 0)
            PANIC;
        if (r = sys_env_set_pgfault_upcall(ceid, _pgfault_upcall), r < 0)
            PANIC;
        if (r = sys_env_set_status(ceid, ENV_RUNNABLE), r < 0)
            PANIC;
    }
    return ceid;
#undef PANIC
}
