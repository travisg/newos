/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>

int arch_cpu_init(kernel_args *ka)
{
	return 0;
}

int arch_cpu_init2(kernel_args *ka)
{
	return 0;
}

void arch_cpu_invalidate_TLB_range(addr start, addr end)
{
}

void arch_cpu_invalidate_TLB_list(addr pages[], int num_pages)
{
}

void arch_cpu_global_TLB_invalidate()
{
}

long long system_time()
{
	return 0;
}
