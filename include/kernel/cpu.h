/*
** Copyright 2002-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _CPU_H
#define _CPU_H

#include <newos/compiler.h>
#include <boot/stage2.h>
#include <kernel/thread.h>
#include <kernel/smp.h>
#include <kernel/timer.h>
#include <kernel/arch/cpu.h>

// one per cpu. put per cpu stuff here, to keep the data local and reduce
// the amount of cache thrashing that goes on otherwise.
typedef struct cpu_ent {
    int cpu_num;

    // thread.c: used to force a reschedule at quantum expiration time
    int preempted;
    struct timer_event quantum_timer;

    // remember which thread's fpu state we hold
    // NULL means we dont hold any state
    struct thread *fpu_state_thread;

    // timer.c: per-cpu timer queues
    struct timer_event * volatile timer_events;
    spinlock_t timer_spinlock;

    // arch-specific stuff
    struct arch_cpu_info arch;
} cpu_ent _ALIGNED(64);

extern cpu_ent cpu[_MAX_CPUS];

int cpu_preboot_init(kernel_args *ka);
int cpu_init(kernel_args *ka);
int cpu_init_percpu(kernel_args *ka, int curr_cpu);

static inline cpu_ent *get_cpu_struct(int cpu_num) { return &cpu[cpu_num]; }

static inline cpu_ent *get_curr_cpu_struct(void)
{
    struct thread *t = thread_get_current_thread();
    if (t)
        return t->cpu;
    else {
        return get_cpu_struct(smp_get_current_cpu());
    }
}

void cpu_spin(bigtime_t interval);

/* manual implementations of the atomic_* ops */
int user_atomic_add(int *val, int incr);
int user_atomic_and(int *val, int incr);
int user_atomic_or(int *val, int incr);
int user_atomic_set(int *val, int set_to);
int user_test_and_set(int *val, int set_to, int test_val);

#endif

