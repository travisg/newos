/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _TIMER_H
#define _TIMER_H

#include <boot/stage2.h>

typedef int (*timer_callback)(void *);

typedef enum {
	TIMER_MODE_ONESHOT = 0,
	TIMER_MODE_PERIODIC
} timer_mode;

struct timer_event {
	struct timer_event *next;
	timer_mode mode;
	time_t sched_time;
	time_t periodic_time;
	timer_callback func;
	void *data;
};

int timer_init(kernel_args *ka);
int timer_interrupt();
void timer_setup_timer(timer_callback func, void *data, struct timer_event *event);
int timer_set_event(time_t relative_time, timer_mode mode, struct timer_event *event);
int timer_cancel_event(struct timer_event *event);

#endif

