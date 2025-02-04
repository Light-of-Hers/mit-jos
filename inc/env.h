/* See COPYRIGHT for copyright information. */

#ifndef JOS_INC_ENV_H
#define JOS_INC_ENV_H

#include <inc/types.h>
#include <inc/trap.h>
#include <inc/memlayout.h>
#include <inc/elink.h>
#include <inc/config.h>

typedef int32_t envid_t;

// An environment ID 'envid_t' has three parts:
//
// +1+---------------21-----------------+--------10--------+
// |0|          Uniqueifier             |   Environment    |
// | |                                  |      Index       |
// +------------------------------------+------------------+
//                                       \--- ENVX(eid) --/
//
// The environment index ENVX(eid) equals the environment's index in the
// 'envs[]' array.  The uniqueifier distinguishes environments that were
// created at different times, but share the same environment index.
//
// All real environments are greater than 0 (so the sign bit is zero).
// envid_ts less than 0 signify errors.  The envid_t == 0 is special, and
// stands for the current environment.

#define LOG2NENV		10
#define NENV			(1 << LOG2NENV)
#define ENVX(envid)		((envid) & (NENV - 1))

// Values of env_status in struct Env
enum {
	ENV_FREE = 0,
	ENV_DYING,
	ENV_RUNNABLE,
	ENV_RUNNING,
	ENV_NOT_RUNNABLE
};

// Special environment types
enum EnvType {
	ENV_TYPE_USER = 0,
	ENV_TYPE_FS,		// File system server
	ENV_TYPE_NS,		// Network server
    ENV_TYPE_SPST,  // snapshot
};

struct Env {
    struct Trapframe env_tf; // Saved registers
    struct Env *env_link;    // Next free Env
    envid_t env_id;          // Unique environment identifier
    envid_t env_parent_id;   // env_id of this env's parent
    enum EnvType env_type;   // Indicates special system environments
    unsigned env_status;     // Status of the environment
    uint32_t env_runs;       // Number of times environment has run
    int env_cpunum;          // The CPU that the env is running on

    // Address space
    pde_t *env_pgdir; // Kernel virtual address of page dir

    // Exception handling
    void *env_pgfault_upcall; // Page fault upcall entry point

#ifdef CONF_MFQ
    EmbedLink env_mfq_link;
    uint32_t env_mfq_level;
    int env_mfq_left_ticks;
#endif

    EmbedLink env_spst_link;    // Embeded link to the snapshot list
    envid_t env_spst_owner_id;  // snapshot's owner env's id
    uint32_t env_spst_dmail;    // DeLorean Mail（这一切都是命运石之门的选择!）
#define EMPTY_DMAIL ~0U

    // Lab 4 IPC
    EmbedLink env_ipc_link;         // Embeded link to the blocking queue.
    EmbedLink env_ipc_queue;        // Blocking queue.
    bool env_ipc_recving;           // Env is blocked receiving
    void *env_ipc_dstva;            // VA at which to map received page
    uint32_t env_ipc_value;         // Data value sent to us
    envid_t env_ipc_from;           // envid of the sender
    int env_ipc_perm;               // Perm of page mapping received
};

#endif // !JOS_INC_ENV_H
