#include <stage2.h>
#include <kernel.h>

#include <arch_int.h>
#include <arch_interrupts.h>
#include <arch_cpu.h>
#include <arch_smp.h>

#include <console.h>
#include <debug.h>
#include <timer.h>
#include <int.h>

#define pit_clock_rate 1193180
#define pit_max_timer_interval ((long long)0xffff * 1000000 / pit_clock_rate)

static void set_isa_hardware_timer(long long relative_timeout)
{
	unsigned short next_event_clocks;

	if (relative_timeout <= 0)
		next_event_clocks = 2;			
	else if (relative_timeout < pit_max_timer_interval)
		next_event_clocks = relative_timeout * pit_clock_rate / 1000000;
	else
		next_event_clocks = 0xffff;

	outb(0x30, 0x43);		
	outb(next_event_clocks & 0xff, 0x40);
	outb((next_event_clocks >> 8) & 0xff, 0x40);
}

static void clear_isa_hardware_timer()
{
	// XXX do something here
}

static int isa_timer_interrupt()
{
	return timer_interrupt();
}

int apic_timer_interrupt()
{
	return timer_interrupt();
}

void arch_timer_set_hardware_timer(time_t timeout)
{
	// try the apic timer first
	if(arch_smp_set_apic_timer(timeout) < 0) {
		set_isa_hardware_timer(timeout);
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
	
	int_set_io_interrupt_handler(0x20, &isa_timer_interrupt);
	// apic timer interrupt set up by smp code

	return 0;
}
