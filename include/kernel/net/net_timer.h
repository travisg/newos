/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_NET_NET_TIMER_H
#define _NEWOS_KERNEL_NET_NET_TIMER_H

#include <kernel/kernel.h>
#include <kernel/lock.h>

typedef void (*net_timer_callback)(void *);

typedef struct net_timer_event {
	struct net_timer_event *next;
	struct net_timer_event *prev;

	net_timer_callback func;
	void *args;

	bigtime_t sched_time;

	bool pending;
} net_timer_event;

int net_timer_init(void);

int set_net_timer(net_timer_event *e, unsigned int delay_ms, net_timer_callback callback, void *args);
int cancel_net_timer(net_timer_event *e);

void clear_net_timer(net_timer_event *e);
extern inline void clear_net_timer(net_timer_event *e)
{
	e->prev = e->next = NULL;
	e->pending = false;
}

#endif

