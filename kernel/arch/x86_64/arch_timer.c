/* 
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <kernel/kernel.h>

#include <sys/types.h>

#include <kernel/arch/int.h>
#include <kernel/arch/cpu.h>

#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/timer.h>
#include <kernel/int.h>

#include <kernel/arch/timer.h>

#include <kernel/arch/x86_64/interrupts.h>
#include <kernel/arch/x86_64/smp_priv.h>
#include <kernel/arch/x86_64/timer.h>

#define pit_clock_rate 1193180
#define pit_max_timer_interval ((bigtime_t)0xffff * 1000000 / pit_clock_rate)

static void set_isa_hardware_timer(bigtime_t relative_timeout, int type)
{
	uint16 next_ticks;

	if (relative_timeout <= 0)
		next_ticks = 2;			
	else if (relative_timeout < pit_max_timer_interval)
		next_ticks = relative_timeout * pit_clock_rate / 1000000;
	else
		next_ticks = 0xffff;

	if(type == HW_TIMER_ONESHOT) {
		out8(0x30, 0x43); // mode 0 (countdown then stop), load LSB then MSB
		out8(next_ticks & 0xff, 0x40);
		out8((next_ticks >> 8) & 0xff, 0x40);
	} else if(type == HW_TIMER_REPEATING) {
		out8(0x36, 0x43); // mode 3 (square wave generator), load LSB then MSB
		out8(next_ticks & 0xff, 0x40);
		out8((next_ticks >> 8) & 0xff, 0x40);
	}

	arch_int_enable_io_interrupt(0);
}

static void clear_isa_hardware_timer(void)
{
	// mask out the timer
	arch_int_disable_io_interrupt(0);
}

static int isa_timer_interrupt(void* data)
{
	return timer_interrupt();
}

int apic_timer_interrupt(void)
{
	return timer_interrupt();
}

void arch_timer_set_hardware_timer(bigtime_t timeout, int type)
{
	// try the apic timer first
	if(arch_smp_set_apic_timer(timeout, type) < 0) {
		set_isa_hardware_timer(timeout, type);
	}
}

void arch_timer_clear_hardware_timer()
{
	if(arch_smp_clear_apic_timer() < 0) {
		clear_isa_hardware_timer();
	}
}

int arch_init_timer(kernel_args *ka)
{
	dprintf("arch_init_timer: entry\n");

	int_set_io_interrupt_handler(0, &isa_timer_interrupt, NULL, "PIT timer");
	clear_isa_hardware_timer();
	// apic timer interrupt set up by smp code

	return 0;
}
