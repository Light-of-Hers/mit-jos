#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/log.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

#ifndef CONF_MFQ
// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment. Make sure curenv is not null before
	// dereferencing it.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// LAB 4: Your code here.
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

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
        for (i = 0; i < NENV; ++i) {
            if (envs[i].env_status == ENV_NOT_RUNNABLE)
                env_destroy(&envs[i]);
        }
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		// Uncomment the following line after completing exercise 13
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}

