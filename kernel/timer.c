#include <kernel.h>
#include <stage2.h>
#include <console.h>
#include <debug.h>
#include <thread.h>
#include <int.h>
#include <smp.h>
#include <vm.h>
#include <timer.h>

#include <arch_cpu.h>
#include <arch_timer.h>

static struct timer_event *events = NULL;
static int timer_spinlock = 0;

int timer_init(kernel_args *ka)
{
	dprintf("init_timer: entry\n");

	return arch_init_timer(ka);
}

static void add_event_to_list(struct timer_event *event, struct timer_event **list)
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
	time_t curr_time = system_time();
	struct timer_event *event;
	
	acquire_spinlock(&timer_spinlock);
		
restart_scan:
	event = events;
	if(event != NULL && event->sched_time < curr_time) {
		// this event needs to happen
		int mode = event->mode;
		
		events = event->next;

		release_spinlock(&timer_spinlock);			

		// call the callback
		// note: if the event is not periodic, it is ok
		// to delete the event structure inside the callback
		if(event->func != NULL)
			event->func(event->data);

		acquire_spinlock(&timer_spinlock);
	
		if(mode == TIMER_MODE_PERIODIC) {
			// we need to adjust it and add it back to the list
			event->sched_time = system_time() + event->periodic_time;
			add_event_to_list(event, &events);
		}			

		goto restart_scan; // the list may have changed
	}

	// setup the next hardware timer
	if(events != NULL)
		arch_timer_set_hardware_timer(events->sched_time - system_time());
	
	release_spinlock(&timer_spinlock);

	return INT_RESCHEDULE;
}

void timer_setup_timer(void (*func)(void *), void *data, struct timer_event *event)
{
	event->func = func;
	event->data = data;
}

int timer_set_event(time_t relative_time, int mode, struct timer_event *event)
{
	int state;
	
	if(event == NULL)
		return -1;

	event->sched_time = system_time() + relative_time;
	event->mode = mode;
	if(event->mode == TIMER_MODE_PERIODIC)
		event->periodic_time = relative_time;
	
	state = int_disable_interrupts();
	acquire_spinlock(&timer_spinlock);

	add_event_to_list(event, &events);
	
	// if we were stuck at the head of the list, set the hardware timer
	if(event == events) {
		arch_timer_set_hardware_timer(relative_time);
	}

	release_spinlock(&timer_spinlock);
	int_restore_interrupts(state);
	
	return 0;
}
