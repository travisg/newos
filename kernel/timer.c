#include "console.h"
#include "debug.h"
#include "thread.h"

#include "arch_timer.h"

int timer_init(struct kernel_args *ka)
{
	dprintf("init_timer: entry\n");
	return arch_init_timer(ka);
}

void timer_interrupt()
{
	thread_timer_interrupt();	
}

