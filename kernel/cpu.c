/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/cpu.h>
#include <kernel/arch/cpu.h>
#include <boot/stage2.h>

#include <nulibc/string.h>

/* global per-cpu structure */
cpu_ent cpu[MAX_BOOT_CPUS];

int cpu_init(kernel_args *ka)
{
	int i;

	memset(cpu, 0, sizeof(cpu));
	for(i = 0; i < MAX_BOOT_CPUS; i++) {
		cpu[i].cpu_num = i;
	}

	return arch_cpu_init(ka);
}

int cpu_preboot_init(kernel_args *ka)
{
	return arch_cpu_preboot_init(ka);
}

