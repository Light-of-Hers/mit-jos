// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/log.h>
#include <inc/assert.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

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
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    envid_t eid;
    pte_t pte;
    
    eid = sys_getenvid();
    pte = uvpt[(uint32_t)addr / PGSIZE];
    addr = ROUNDDOWN(addr, PGSIZE);

    if (!(err & FEC_WR) || !(pte & PTE_COW))
        panic(
            "[0x%08x] user page fault va 0x%08x ip 0x%08x\n"
            "[%s, %s, %s]",
            eid,
            utf->utf_fault_va,
            utf->utf_eip,
            err & 4 ? "user" : "kernel",
            err & 2 ? "write" : "read",
            err & 1 ? "protection" : "not-present"
        );

#define PANIC panic("page fault handler: %e", r)
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    if (r = sys_page_alloc(eid, PFTEMP, PTE_P | PTE_U | PTE_W), r < 0)
        PANIC;
    memmove(PFTEMP, addr, PGSIZE);
    if (r = sys_page_map(eid, PFTEMP, eid, addr, PTE_P | PTE_U | PTE_W), r < 0)
        PANIC;
    if (r = sys_page_unmap(eid, PFTEMP), r < 0)
        PANIC;
    return;
	// panic("pgfault not implemented");
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
    envid_t peid;
    void *pg;
    pte_t pte;

    peid = sys_getenvid();
    pg = (void*)(pn * PGSIZE);
    pte = uvpt[pn];

    assert(pte & PTE_P && pte & PTE_U);
    if (pte & PTE_W || pte & PTE_COW) {
        if (r = sys_page_map(peid, pg, envid, pg, PTE_P | PTE_U | PTE_COW), r < 0)
            return r;
        if (r = sys_page_map(peid, pg, peid, pg, PTE_P | PTE_U | PTE_COW), r < 0)
            return r;
    } else {
        if (r = sys_page_map(peid, pg, envid, pg, PTE_P | PTE_U), r < 0)
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
	// LAB 4: Your code here.
	// panic("fork not implemented");
    int r;
    envid_t ceid;
#define PANIC panic("fork: %e", r)
    set_pgfault_handler(pgfault);
    if (ceid = sys_exofork(), ceid > 0) {
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

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
