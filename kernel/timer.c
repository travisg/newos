#include "console.h"
#include "debug.h"
#include "thread.h"
#include "int.h"
#include "smp.h"

#include "arch_timer.h"

int timer_init(struct kernel_args *ka)
{
	dprintf("init_timer: entry\n");
	return arch_init_timer(ka);
}

int timer_interrupt()
{
	// XXX remove -- tell the other processor to reschedule too
	if(smp_get_num_cpus() > 1)
		smp_send_ici(smp_get_current_cpu() == 0 ? 1 : 0, SMP_MSG_RESCHEDULE, 0, NULL);

	return INT_RESCHEDULE;
}

