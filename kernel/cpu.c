/*
** Copyright 2002-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/cpu.h>
#include <kernel/vm.h>
#include <kernel/time.h>
#include <boot/stage2.h>

#include <string.h>

/* global per-cpu structure */
cpu_ent cpu[_MAX_CPUS];

int cpu_init(kernel_args *ka)
{
	int i;

	memset(cpu, 0, sizeof(cpu));
	for(i = 0; i < _MAX_CPUS; i++) {
		cpu[i].info.cpu_num = i;
	}

	return arch_cpu_init(ka);
}

int cpu_init_percpu(kernel_args *ka, int curr_cpu)
{
	return arch_cpu_init_percpu(ka, curr_cpu);
}

int cpu_preboot_init(kernel_args *ka)
{
	return arch_cpu_preboot_init(ka);
}

void cpu_spin(bigtime_t interval)
{
	bigtime_t start = system_time();

	while(system_time() - start < interval)
		;
}

int user_atomic_add(int *uval, int incr)
{
	int val;
	int ret;

	if(is_kernel_address(uval))
		goto error;

	if(user_memcpy(&val, uval, sizeof(val)) < 0)
		goto error;

	ret = val;
	val += incr;

	if(user_memcpy(uval, &val, sizeof(val)) < 0)
		goto error;

	return ret;

error:
	// XXX kill the app
	return -1;
}

int user_atomic_and(int *uval, int incr)
{
	int val;
	int ret;

	if(is_kernel_address(uval))
		goto error;

	if(user_memcpy(&val, uval, sizeof(val)) < 0)
		goto error;

	ret = val;
	val &= incr;

	if(user_memcpy(uval, &val, sizeof(val)) < 0)
		goto error;

	return ret;

error:
	// XXX kill the app
	return -1;
}

int user_atomic_or(int *uval, int incr)
{
	int val;
	int ret;

	if(is_kernel_address(uval))
		goto error;

	if(user_memcpy(&val, uval, sizeof(val)) < 0)
		goto error;

	ret = val;
	val |= incr;

	if(user_memcpy(uval, &val, sizeof(val)) < 0)
		goto error;

	return ret;

error:
	// XXX kill the app
	return -1;
}

int user_atomic_set(int *uval, int set_to)
{
	int val;
	int ret;

	if(is_kernel_address(uval))
		goto error;

	if(user_memcpy(&val, uval, sizeof(val)) < 0)
		goto error;

	ret = val;
	val = set_to;

	if(user_memcpy(uval, &val, sizeof(val)) < 0)
		goto error;

	return ret;

error:
	// XXX kill the app
	return -1;
}

int user_test_and_set(int *uval, int set_to, int test_val)
{
	int val;
	int ret;

	if(is_kernel_address(uval))
		goto error;

	if(user_memcpy(&val, uval, sizeof(val)) < 0)
		goto error;

	ret = val;
	if(val == test_val) {
		val = set_to;
		if(user_memcpy(uval, &val, sizeof(val)) < 0)
			goto error;
	}

	return ret;

error:
	// XXX kill the app
	return -1;
}

