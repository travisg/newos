#ifndef _TIMER_H
#define _TIMER_H

#include <stage2.h>

struct timer_event {
	struct timer_event *next;
	int mode;
	time_t sched_time;
	time_t periodic_time;
	void (*func)(void *);
	void *data;
};

enum {
	TIMER_MODE_ONESHOT = 0,
	TIMER_MODE_PERIODIC
};

int timer_init(kernel_args *ka);
int timer_interrupt();
void timer_setup_timer(void (*func)(void *), void *data, struct timer_event *event);
int timer_set_event(time_t relative_time, int mode, struct timer_event *event);
int timer_cancel_event(struct timer_event *event);

#endif

