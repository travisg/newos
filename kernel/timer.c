/*
** Copyright 2001-2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/int.h>
#include <kernel/smp.h>
#include <kernel/vm.h>
#include <kernel/timer.h>
#include <newos/errors.h>
#include <boot/stage2.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/timer.h>
#include <kernel/arch/smp.h>

int timer_init(kernel_args *ka)
{
	dprintf("init_timer: entry\n");

	return arch_init_timer(ka);
}

// NOTE: expects interrupts to be off
static void add_event_to_list(struct timer_event *event, struct timer_event * volatile *list)
{
	struct timer_event *next;
	struct timer_event *last = NULL;

	// stick it in the event list
	next = *list;
	while(next != NULL && next->sched_time < event->sched_time) {
		last = next;
		next = next->next;
	}

	if(last != NULL) {
		event->next = last->next;
		last->next = event;
	} else {
		event->next = next;
		*list = event;
	}
}

int timer_interrupt()
{
	bigtime_t curr_time = system_time();
	struct timer_event *event;
	spinlock_t *spinlock;
	cpu_ent *cpu = get_curr_cpu_struct();
	int rc = INT_NO_RESCHEDULE;

//	dprintf("timer_interrupt: time 0x%x 0x%x, cpu %d\n", system_time(), smp_get_current_cpu());

	spinlock = &cpu->info.timer_spinlock;

	acquire_spinlock(spinlock);

restart_scan:
	event = cpu->info.timer_events;
	if(event != NULL && event->sched_time < curr_time) {
		// this event needs to happen
		int mode = event->mode;

		cpu->info.timer_events = event->next;
		event->sched_time = 0;

		release_spinlock(spinlock);

		// call the callback
		// note: if the event is not periodic, it is ok
		// to delete the event structure inside the callback
		if(event->func != NULL) {
			if(event->func(event->data) == INT_RESCHEDULE)
				rc = INT_RESCHEDULE;
		}

		acquire_spinlock(spinlock);

		if(mode == TIMER_MODE_PERIODIC) {
			// we need to adjust it and add it back to the list
			event->sched_time = system_time() + event->periodic_time;
			if(event->sched_time == 0)
				event->sched_time = 1; // if we wrapped around and happen
				                       // to hit zero, set it to one, since
				                       // zero represents not scheduled
			add_event_to_list(event, &cpu->info.timer_events);
		}

		goto restart_scan; // the list may have changed
	}

	// setup the next hardware timer
	if(cpu->info.timer_events != NULL)
		arch_timer_set_hardware_timer(cpu->info.timer_events->sched_time - system_time());

	release_spinlock(spinlock);

	return rc;
}

void timer_setup_timer(timer_callback func, void *data, struct timer_event *event)
{
	event->func = func;
	event->data = data;
	event->sched_time = 0;
}

int timer_set_event(bigtime_t relative_time, timer_mode mode, struct timer_event *event)
{
	cpu_ent *cpu;

	if(event == NULL)
		return ERR_INVALID_ARGS;

	if(relative_time < 0)
		relative_time = 0;

	if(event->sched_time != 0)
		panic("timer_set_event: event %p in list already!\n", event);

	event->sched_time = system_time() + relative_time;
	if(event->sched_time == 0)
		event->sched_time = 1; // if we wrapped around and happen
		                       // to hit zero, set it to one, since
		                       // zero represents not scheduled
	event->mode = mode;
	if(event->mode == TIMER_MODE_PERIODIC)
		event->periodic_time = relative_time;

	int_disable_interrupts();

	cpu = get_curr_cpu_struct();

	acquire_spinlock(&cpu->info.timer_spinlock);

	event->scheduled_cpu = cpu->info.cpu_num;
	add_event_to_list(event, &cpu->info.timer_events);

	// if we were stuck at the headof the list, set the hardware timer
	if(event == cpu->info.timer_events) {
		arch_timer_set_hardware_timer(relative_time);
	}

	release_spinlock(&cpu->info.timer_spinlock);
	int_restore_interrupts();

	return 0;
}

/* this is a fast path to be called from reschedule and from timer_cancel_event */
/* must always be invoked with interrupts disabled */
int _local_timer_cancel_event(int curr_cpu, struct timer_event *event)
{
	struct timer_event *last = NULL;
	struct timer_event *e;
	bool foundit = false;
	cpu_ent *cpu = get_cpu_struct(curr_cpu);

	acquire_spinlock(&cpu->info.timer_spinlock);
	e = cpu->info.timer_events;
	while(e != NULL) {
		if(e == event) {
			// we found it
			foundit = true;
			if(e == cpu->info.timer_events) {
				cpu->info.timer_events = e->next;
			} else {
				last->next = e->next;
			}
			e->next = NULL;
			// break out of the whole thing
			goto done;
		}
		last = e;
		e = e->next;
	}
	release_spinlock(&cpu->info.timer_spinlock);
done:

	if(cpu->info.timer_events == NULL) {
		arch_timer_clear_hardware_timer();
	} else {
		arch_timer_set_hardware_timer(cpu->info.timer_events->sched_time - system_time());
	}

	if(foundit) {
		release_spinlock(&cpu->info.timer_spinlock);
		event->sched_time = 0;
	}

	return (foundit ? 0 : ERR_GENERAL);
}

int local_timer_cancel_event(struct timer_event *event)
{
	return _local_timer_cancel_event(smp_get_current_cpu(), event);
}

int timer_cancel_event(struct timer_event *event)
{
	struct timer_event *last;
	struct timer_event *e;
	bool foundit = false;
	int num_cpus = smp_get_num_cpus();
	int curr_cpu_num;
	cpu_ent *cpu;

	if(event->sched_time == 0)
		return 0; // it's not scheduled

	int_disable_interrupts();
	curr_cpu_num = smp_get_current_cpu();

	// check to see if this structure is sane
	if(event->sched_time == 0)
		goto done;
	ASSERT(event->scheduled_cpu >= 0 && event->scheduled_cpu < num_cpus);

	// check the queue the event was supposed to be in
	if(event->scheduled_cpu == curr_cpu_num) {
		if(_local_timer_cancel_event(curr_cpu_num, event) == 0)
			foundit = true;
	} else {
		// it should be in another cpu's queue
		cpu = get_cpu_struct(event->scheduled_cpu);
		acquire_spinlock(&cpu->info.timer_spinlock);
		last = NULL;
		e = cpu->info.timer_events;
		while(e != NULL) {
			if(e == event) {
				// we found it
				foundit = true;
				if(e == cpu->info.timer_events) {
					cpu->info.timer_events = e->next;
				} else {
					last->next = e->next;
				}
				e->sched_time = 0;
				e->next = NULL;
				// break out of the whole thing
				break;
			}
			last = e;
			e = e->next;
		}
		release_spinlock(&cpu->info.timer_spinlock);
	}

done:
	int_restore_interrupts();

	return (foundit ? 0 : ERR_GENERAL);
}
