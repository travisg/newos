#include "stage2.h"

#include "arch_int.h"
#include "arch_interrupts.h"
#include "arch_cpu.h"

#include "console.h"
#include "debug.h"

#define pit_clock_rate 1193180
#define pit_max_timer_interval ((long long)0xffff * 1000000 / pit_clock_rate)

static long long periodic_timer_rate = 10000; // us

// defined in ../../timer.c
void timer_interrupt();

int arch_init_timer(struct kernel_args *ka)
{
	dprintf("arch_init_timer: entry\n");
	
	set_intr_gate(32, &_timer_int);

	return 0;
}

static void set_hardware_timer(long long relative_timeout)
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

void i386_timer_interrupt()
{
	timer_interrupt();

	// handle setting it up for the next shot
	set_hardware_timer(periodic_timer_rate);		
}

