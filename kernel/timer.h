#ifndef _TIMER_H
#define _TIMER_H

int timer_init(struct kernel_args *ka);
int timer_interrupt();
int timer_set_event(long long relative_time, int mode, void (*func)(void *), void *data);

enum {
	TIMER_MODE_ONESHOT = 0,
	TIMER_MODE_PERIODIC
};

#endif

