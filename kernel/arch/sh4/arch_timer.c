#include <kernel.h>
#include <stage2.h>
#include <sh4.h>
#include <int.h>
#include <debug.h>
#include <arch_cpu.h>

static void start_timer(int timer)
{
	uint8 old_val = *(uint8 *)TSTR;
	*(uint8 *)TSTR = old_val | (0x1 << timer % 3);
}

static void stop_timer(int timer)
{
	uint8 old_val = *(uint8 *)TSTR;
	*(uint8 *)TSTR = old_val & ~(0x1 << timer %3);
}

void arch_timer_set_hardware_timer(time_t timeout)
{
	return;
}

void arch_timer_clear_hardware_timer()
{
	return;
}

static int timer_interrupt()
{
	dprintf("timer interrupt!\n");
	return INT_NO_RESCHEDULE;
}

int arch_init_timer(kernel_args *ka)
{
	int i;
	uint8 old_val;
	dprintf("arch_init_timer: entry\n");

	int_set_io_interrupt_handler(32, &timer_interrupt);

	dprintf("sr = 0x%x\n", get_sr());		
	int_enable_interrupts();
	dprintf("sr = 0x%x\n", get_sr());		

	// play with one of the timers
	dprintf("TCOR0 = 0x%x\n", *(uint32 *)TCOR0);	
	dprintf("TCNT0 = 0x%x\n", *(uint32 *)TCNT0);	
	dprintf("TCR0  = 0x%x\n", *(uint16 *)TCR0);

	dprintf("TSTR  = 0x%x\n", *(uint8 *)TSTR);
	stop_timer(0);
	*(uint16 *)TCR0 = 0x0020;
	*(uint32 *)TCNT0 = 200;
	*(uint32 *)TCOR0 = 200;
	start_timer(0);
	dprintf("TSTR  = 0x%x\n", *(uint8 *)TSTR);

	dprintf("TCOR0 = 0x%x\n", *(uint32 *)TCOR0);	
	dprintf("TCNT0 = 0x%x\n", *(uint32 *)TCNT0);	
	dprintf("TCR0  = 0x%x\n", *(uint16 *)TCR0);

	dprintf("TSTR  = 0x%x\n", *(uint8 *)TSTR);

	dprintf("TCOR0 = 0x%x\n", *(uint32 *)TCOR0);	
	dprintf("TCNT0 = 0x%x\n", *(uint32 *)TCNT0);	
	dprintf("TCR0  = 0x%x\n", *(uint16 *)TCR0);

	return 0;
}

