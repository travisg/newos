/* 
** Copyright 2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/arch/time.h>

int arch_time_init(kernel_args *ka)
{
	return 0;
}

void arch_time_tick(void)
{
}

bigtime_t arch_get_time_delta(void)
{
	return 0;
}

bigtime_t arch_get_rtc_delta(void)
{
	return 0;
}

