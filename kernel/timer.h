#ifndef _TIMER_H
#define _TIMER_H

#include <stage2.h>

int timer_init(kernel_args *ka);
int timer_interrupt();
int timer_set_event(time_t relative_time, int mode, void (*func)(void *), void *data);

enum {
	TIMER_MODE_ONESHOT = 0,
	TIMER_MODE_PERIODIC
};

#endif

