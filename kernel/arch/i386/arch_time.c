/* 
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/arch/time.h>

static uint64 last_rdtsc;
static bool use_rdtsc;

int arch_time_init(kernel_args *ka)
{
	last_rdtsc = 0;

	use_rdtsc = ka->arch_args.supports_rdtsc;

	return 0;
}

void arch_time_tick(void)
{
	if(use_rdtsc)
		last_rdtsc = i386_rdtsc();
}

bigtime_t arch_get_time_delta(void)
{
	if(use_rdtsc) {
		// XXX race here
		uint64 delta_rdtsc = i386_rdtsc() - last_rdtsc;

		return i386_cycles_to_time(delta_rdtsc);
	} else 
		return 0;
}

