/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <kernel/kernel.h>
#include <kernel/debug.h>

#include <kernel/timer.h>
#include <kernel/arch/timer.h>

static bigtime_t tick_rate;
static uint32 periodic_tick_val;

void arch_timer_set_hardware_timer(bigtime_t timeout, int type)
{
	bigtime_t new_val_64;

	if(timeout < 1000)
		timeout = 1000;

	new_val_64 = (timeout * tick_rate) / 1000000;

	asm("mtdec	%0" :: "r"((uint32)new_val_64));
	
	periodic_tick_val = new_val_64;
}

void ppc_timer_reset(void)
{
	asm("mtdec	%0" :: "r"((uint32)periodic_tick_val));
}

void arch_timer_clear_hardware_timer()
{
	asm("mtdec	%0" :: "r"(0x7fffffff));
}

int arch_init_timer(kernel_args *ka)
{
	tick_rate = (66*1000*1000) / 4; // XXX fix

	return 0;
}
