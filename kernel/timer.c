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
#include <arch_smp.h>

static struct timer_event *events[SMP_MAX_CPUS] = { NULL, };
static int timer_spinlock[SMP_MAX_CPUS] = { 0, };

int timer_init(kernel_args *ka)
{
	TOUCH(ka);
	
	dprintf("init_timer: entry\n");
	
	return arch_init_timer(ka);
}

// NOTE: expects interrupts to be off
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
	struct timer_event **list;
	int *spinlock;
	int curr_cpu = smp_get_current_cpu();
	
//	dprintf("timer_interrupt: time 0x%x 0x%x, cpu %d\n", system_time(), smp_get_current_cpu());

	list = &events[curr_cpu];
	spinlock = &timer_spinlock[curr_cpu];
	
	acquire_spinlock(spinlock);
		
restart_scan:
	event = *list;
	if(event != NULL && event->sched_time < curr_time) {
		// this event needs to happen
		int mode = event->mode;
		
		*list = event->next;
		event->next = NULL;

		release_spinlock(spinlock);			

		// call the callback
		// note: if the event is not periodic, it is ok
		// to delete the event structure inside the callback
		if(event->func != NULL)
			event->func(event->data);

		acquire_spinlock(spinlock);
	
		if(mode == TIMER_MODE_PERIODIC) {
			// we need to adjust it and add it back to the list
			event->sched_time = system_time() + event->periodic_time;
			add_event_to_list(event, list);
		}			

		goto restart_scan; // the list may have changed
	}

	// XXX need to deal with the volatility of the list pointer
	// the compiler may cache *list and it may have changed.

	// setup the next hardware timer
	if(*list != NULL)
		arch_timer_set_hardware_timer((*list)->sched_time - system_time());
	
	release_spinlock(spinlock);

	return INT_RESCHEDULE;
}

void timer_setup_timer(void (*func)(void *), void *data, struct timer_event *event)
{
	event->func = func;
	event->data = data;
	event->next = NULL;
}

int timer_set_event(time_t relative_time, int mode, struct timer_event *event)
{
	int state;
	struct timer_event **list;
	int curr_cpu;
	
	if(event == NULL)
		return -1;

	if(event->next != NULL)
		panic("timer_set_event: event 0x%x in list already!\n", event);

	event->sched_time = system_time() + relative_time;
	event->mode = mode;
	if(event->mode == TIMER_MODE_PERIODIC)
		event->periodic_time = relative_time;
	
	state = int_disable_interrupts();

	curr_cpu = smp_get_current_cpu();

	list = (struct timer_event **)&events[curr_cpu];

	acquire_spinlock(&timer_spinlock[curr_cpu]);

	add_event_to_list(event, list);

	// if we were stuck at the head of the list, set the hardware timer
	if(event == *list) {
		arch_timer_set_hardware_timer(relative_time);
	}

	release_spinlock(&timer_spinlock[curr_cpu]);
	int_restore_interrupts(state);
	
	return 0;
}

int timer_cancel_event(struct timer_event *event)
{
	int state;
	struct timer_event *last = NULL;
	struct timer_event *e;
	struct timer_event **list = NULL;
	bool reset_timer = false;
	bool foundit = false;
	int num_cpus = smp_get_num_cpus();
	int cpu;
	int curr_cpu;

	state = int_disable_interrupts();
	curr_cpu = smp_get_current_cpu();
	acquire_spinlock(&timer_spinlock[curr_cpu]);

	// walk through all of the cpu's timer queues
	for(cpu = 0; cpu < num_cpus; cpu++) {
		list = (struct timer_event **)&events[cpu];
		
		e = *list;
		while(e != NULL) {
			if(e == event) {
				// we found it
				foundit = true;
				if(e == *list) {
					*list = e->next;
					// we'll need to reset the local timer if
					// this is in the local timer queue
					if(cpu == curr_cpu) 
						reset_timer = true;
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
	}
done:

	if(reset_timer == true) {
		if(*list == NULL) {
			arch_timer_clear_hardware_timer();
		} else {
			arch_timer_set_hardware_timer((*list)->sched_time - system_time());
		}
	}

	release_spinlock(&timer_spinlock[curr_cpu]);
	int_restore_interrupts(state);
	
	return (foundit ? 0 : -1);
}
