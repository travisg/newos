/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/time.h>
#include <kernel/arch/time.h>

// The 'rough' counter of system up time, accurate to the rate at
// which the timer interrupts run (see TICK_RATE in timer.c).
static bigtime_t sys_time;

int time_init(kernel_args *ka)
{
	dprintf("time_init: entry\n");

	sys_time = 0;

	return arch_time_init(ka);
}

void time_tick(int tick_rate)
{
	sys_time += tick_rate;
	arch_time_tick();
}

bigtime_t system_time(void)
{
	volatile bigtime_t *st = &sys_time;
	bigtime_t val;

retry:
	// read the system time, make sure we didn't get a partial read
	val = sys_time;
	if(val > *st)
		goto retry;

	val += arch_get_time_delta();

	return val;
}

bigtime_t system_time_lores(void)
{
	volatile bigtime_t *st = &sys_time;
	bigtime_t val;

retry:
	// read the system time, make sure we didn't get a partial read
	val = sys_time;
	if(val > *st)
		goto retry;

	return val;
}

