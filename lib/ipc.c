// User-level IPC library routines

#include <inc/lib.h>
#include <inc/config.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
//	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
//	in *perm_store (this is nonzero iff a page was successfully
//	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender
//
// Hint:
//   Use 'thisenv' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value, since that's
//   a perfectly valid place to map a page.)
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

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
//
// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_try_send a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
	// panic("ipc_send not implemented");
    int r;

#ifndef CONF_IPC_SLEEP
    while(r = sys_ipc_try_send(to_env, val, pg ? ROUNDDOWN(pg, PGSIZE) : (void*)UTOP, perm), r == -E_IPC_NOT_RECV)
        sys_yield();
#else
    r = sys_ipc_send(to_env, val, pg ? ROUNDDOWN(pg, PGSIZE) : (void*)UTOP, perm);
#endif

    if (r < 0)
        panic("ipc send: %e", r);
}

// Find the first environment of the given type.  We'll use this to
// find special environments.
// Returns 0 if no such environment exists.
envid_t
ipc_find_env(enum EnvType type)
{
	int i;
	for (i = 0; i < NENV; i++)
		if (envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}
