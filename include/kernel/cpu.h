/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _CPU_H
#define _CPU_H

#include <boot/stage2.h>
#include <kernel/smp.h>
#include <kernel/timer.h>
#include <kernel/arch/cpu.h>

// one per cpu. put per cpu stuff here, to keep the data local and reduce
// the amount of cache thrashing that goes on otherwise.
typedef union cpu_ent {
	struct {
		int cpu_num;

		// thread.c: used to force a reschedule at quantum expiration time
		int preempted;
		struct timer_event quantum_timer;
	} info;

	struct arch_cpu_info arch_info;

	uint32 align[16];
} cpu_ent;

extern cpu_ent cpu[MAX_BOOT_CPUS];

int cpu_preboot_init(kernel_args *ka);
int cpu_init(kernel_args *ka);

cpu_ent *get_curr_cpu_struct(void);
extern inline cpu_ent *get_curr_cpu_struct(void) { return &cpu[smp_get_current_cpu()]; }
cpu_ent *get_cpu_struct(int cpu_num);
extern inline cpu_ent *get_cpu_struct(int cpu_num) { return &cpu[cpu_num]; }

/* manual implementations of the atomic_* ops */
int user_atomic_add(int *val, int incr);
int user_atomic_and(int *val, int incr);
int user_atomic_or(int *val, int incr);
int user_atomic_set(int *val, int set_to);
int user_test_and_set(int *val, int set_to, int test_val);

#endif

