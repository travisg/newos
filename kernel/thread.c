#include <string.h>
#include <printf.h>
#include "kernel.h"
#include "stage2.h"
#include "debug.h"
#include "console.h"
#include "proc.h"
#include "thread.h"
#include "arch_thread.h"
#include "khash.h"
#include "int.h"
#include "smp.h"
#include "timer.h"
#include "arch_cpu.h"
#include "arch_int.h"
#include "spinlock.h"
#include "sem.h"

// global
int thread_spinlock = 0;

static int next_thread_id = 0;

#define CURR_THREAD cur_thread[smp_get_current_cpu()]

static struct thread **cur_thread = NULL;
static struct thread *thread_list = NULL;

// thread queues
static struct thread_queue run_q[THREAD_NUM_PRIORITY_LEVELS] = { { NULL, NULL }, };
static struct thread_queue death_q = { NULL, NULL };

static int _rand();
static void thread_entry(void);

// insert a thread onto the tail of a queue	
void thread_enqueue(struct thread *t, struct thread_queue *q)
{
	t->q_next = NULL;
	if(q->head == NULL) {
		q->head = t;
		q->tail = t;
	} else {
		q->tail->q_next = t;
		q->tail = t;
	}
}

struct thread *thread_lookat_queue(struct thread_queue *q)
{
	return q->head;
}

struct thread *thread_dequeue(struct thread_queue *q)
{
	struct thread *t;

	t = q->head;
	if(t != NULL) {
		q->head = t->q_next;
		if(q->tail == t)
			q->tail = NULL;
	}
	return t;
}

struct thread *thread_dequeue_id(struct thread_queue *q, int thread_id)
{
	struct thread *t;
	struct thread *last = NULL;

	t = q->head;
	while(t != NULL) {
		if(t->id == thread_id) {
			if(last == NULL) {
				q->head = t->q_next;
			} else {
				last->q_next = t->q_next;
			}
			if(q->tail == t)
				q->tail = last;
			break;
		}
		last = t;
		t = t->q_next;
	}
	return t;
}

struct thread *thread_lookat_run_q(int priority)
{
	return thread_lookat_queue(&run_q[priority]);
}

void thread_enqueue_run_q(struct thread *t)
{
	// these shouldn't exist
	if(t->priority > THREAD_MAX_PRIORITY)
		t->priority = THREAD_MAX_PRIORITY;
	if(t->priority < 0)
		t->priority = 0;

	thread_enqueue(t, &run_q[t->priority]);
}

struct thread *thread_dequeue_run_q(int priority)
{
	return thread_dequeue(&run_q[priority]);
}

static void insert_thread_into_proc(struct proc *p, struct thread *t)
{
	t->proc_next = p->thread_list;
	p->thread_list = t;
}

static struct thread *create_thread_struct(char *name)
{
	struct thread *t;
	char sem_name[64];
	
	t = (struct thread *)kmalloc(sizeof(struct thread));
	if(t == NULL)
		goto err;
	t->name = (char *)kmalloc(strlen(name) + 1);
	if(t->name == NULL)
		goto err1;
	strcpy(t->name, name);
	t->id = next_thread_id++;
	t->proc = NULL;
	t->kernel_stack_area = NULL;
	t->user_stack_area = NULL;
	t->proc_next = NULL;
	t->q_next = NULL;
	t->priority = -1;

	sprintf(sem_name, "%s_snooze_sem", name);
	t->snooze_sem_id = sem_create(0, sem_name);
	if(t->snooze_sem_id < 0)
		goto err2;
		
	t->arch_info = arch_thread_create_thread_struct();
	if(t->arch_info == NULL)
		goto err3;
	
	return t;

err3:
	sem_delete(t->snooze_sem_id);
err2:
	kfree(t->name);
err1:
	kfree(t);
err:
	return NULL;
}

static struct thread *create_kernel_thread(char *name, int (*func)(void), int priority)
{
	struct thread *t;
	unsigned int *kstack_addr;
	int state;
	char stack_name[64];
	
	t = create_thread_struct(name);
	if(t == NULL)
		return NULL;
	t->proc = proc_get_kernel_proc();
	t->priority = priority;
	t->state = THREAD_STATE_NEW;
	t->next_state = THREAD_STATE_NEW;

	sprintf(stack_name, "%s_kstack", name);
	vm_create_area(t->proc->aspace, stack_name, (void **)&kstack_addr,
		AREA_ANY_ADDRESS, KSTACK_SIZE, 0);
	t->kernel_stack_area = vm_find_area_by_name(t->proc->aspace, stack_name);
	arch_thread_initialize_kthread_stack(t, func, &thread_entry);
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	// insert into global list
	t->all_next = thread_list;
	thread_list = t;

	insert_thread_into_proc(t->proc, t);

	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);
	
	return t;
}

int thread_killer()
{
	dprintf("thread_killer: manny is alive!\n");

	while(1) {
		int state;
		struct thread *t;

retry:		
		// kill something
		state = int_disable_interrupts();
		GRAB_THREAD_LOCK();
		
		while((t = thread_lookat_queue(&death_q)) != NULL) {
			if(t->state != THREAD_STATE_MARKED_FOR_DEATH) {
				// the thread is probably still running, hasn't rescheded yet
				RELEASE_THREAD_LOCK();
				int_restore_interrupts(state);
				thread_snooze(100000); // 10ms
				goto retry;
			}
			
			kprintf("manny killing thread %d\n", t->id);	
			t = thread_dequeue(&death_q);
			// XXX kill the thread		
		}				
		
		RELEASE_THREAD_LOCK();
		int_restore_interrupts(state);

		thread_snooze(100000); // 100ms			
	}
	return 0;
}

int thread_init(struct kernel_args *ka)
{
	struct thread *t;
	int i;
	
	dprintf("thread_init: entry\n");

	// zero out the run queues
	memset(run_q, 0, sizeof(run_q));

	// allocate as many CUR_THREAD slots as there are cpus
	cur_thread = (struct thread **)kmalloc(sizeof(struct thread *) * smp_get_num_cpus());
	if(cur_thread == NULL)
		return -1;
	memset(cur_thread, 0, sizeof(struct thread *) * smp_get_num_cpus());

	// create an idle thread for each cpu
	for(i=0; i<ka->num_cpus; i++) {
		char temp[64];
		
		sprintf(temp, "idle_thread%d", i);
		t = create_thread_struct(temp);
		t->proc = proc_get_kernel_proc();
		t->priority = THREAD_IDLE_PRIORITY;
		t->state = THREAD_STATE_NEW;
		t->next_state = THREAD_STATE_NEW;
		sprintf(temp, "idle_thread%d_kstack", i);
		t->kernel_stack_area = vm_find_area_by_name(t->proc->aspace, temp);		
		t->all_next = thread_list;
		thread_list = t;
		insert_thread_into_proc(t->proc, t);
		cur_thread[i] = t;
	}

	// create a worker thread to kill other ones
	t = create_kernel_thread("manny", &thread_killer, THREAD_MAX_PRIORITY);
	thread_enqueue_run_q(t);

	// set up a periodic timer (10ms)
	timer_set_event(10000, TIMER_MODE_PERIODIC, NULL, NULL);

	return 0;
}

void thread_snooze(long long time)
{
	sem_acquire_etc(thread_get_current_thread()->snooze_sem_id, 1, SEM_FLAG_TIMEOUT, time);
}

// this function gets run by a new thread before anything else
static void thread_entry(void)
{
	// simulates the thread spinlock release that would occur if the thread had been
	// rescheded from. The resched didn't happen because the thread is new.
//	dprintf("thread_entry: on thread %d\n", thread_get_current_thread_id());
	dprintf("thread_entry: entry\n");
	RELEASE_THREAD_LOCK();
}

int thread_kthread_exit()
{
	int state;
	struct thread *t = CURR_THREAD;
	
	dprintf("kernel thread exiting\n");
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();
	
	// put ourselves into the death queue
	thread_enqueue(t, &death_q);
	t->next_state = THREAD_STATE_MARKED_FOR_DEATH;
	
	thread_resched();

	// never make it here
//	RELEASE_THREAD_LOCK();
//	int_restore_interrupts(state);
}

int thread_get_current_thread_id()
{
	return CURR_THREAD->id;
}

struct thread *thread_get_current_thread()
{
	return CURR_THREAD;
}

static void thread_context_switch(struct thread *t_from, struct thread *t_to)
{
	arch_thread_context_switch(t_from, t_to);
}

/* thread TEST code */
static int thread_test_sem;

int test_thread()
{
	int a = 0;
	int tid = thread_get_current_thread_id();
	int x, y;
	char c = 'a';
	
	x = tid % 80;
	y = (tid / 80) * 2 ;
	
	while(1) {
//		a += tid;
		a++;
//		kprintf_xy(0, tid-1, "thread%d - %d    - %d %d - cpu %d", tid, a, system_time(), smp_get_current_cpu());
		kprintf_xy(x, y, "%c", c++);
		if(c > 'z')
			c = 'a';
		kprintf_xy(x, y+1, "%d", smp_get_current_cpu());
#if 0
		thread_snooze(10000 * tid);
#endif
#if 0
		switch(tid) {
			case 3: case 6:
				if((a % 2048) == 0)
					sem_release(thread_test_sem, 1);
				break;
			default:
				sem_acquire(thread_test_sem, 1);
		}
#endif
#if 0
		if(a > tid * 100) {
			kprintf("thread %d exiting\n", tid);
			break;
		}
#endif
	}
	return 1;
}

int thread_test()
{
	struct thread *t;
	int i;
	char temp[64];
	
	for(i=0; i<64; i++) {
		sprintf(temp, "test_thread%d", i);
//		t = create_kernel_thread(temp, &test_thread, i % THREAD_NUM_PRIORITY_LEVELS);
		t = create_kernel_thread(temp, &test_thread, 5);
		thread_enqueue_run_q(t);
	}	

	thread_test_sem = sem_create(1, "test");
	return 0;
}

static int
_rand()
{
	static int next = 0;

	if(next == 0)
		next = system_time();

	next = next * 1103515245 + 12345;
	return((next >> 16) & 0x7FFF);
}

// NOTE: expects thread_spinlock to be held
void thread_resched()
{
	struct thread *next_thread = NULL;
	int last_thread_pri = -1;
	struct thread *old_thread = CURR_THREAD;
	int i;
	int oldstate;
	
	switch(old_thread->next_state) {
		case THREAD_STATE_RUNNING:
		case THREAD_STATE_NEW:
		case THREAD_STATE_READY:
			thread_enqueue_run_q(old_thread);
			break;
		default:
	}
	old_thread->state = old_thread->next_state;

	for(i = THREAD_MAX_PRIORITY; i > THREAD_IDLE_PRIORITY; i--) {
		next_thread = thread_lookat_run_q(i);
		if(next_thread != NULL) {
			// skip it sometimes
			if(_rand() > 0x3000) {
				next_thread = thread_dequeue_run_q(i);
				break;
			}
			last_thread_pri = i;
			next_thread = NULL;
		}
	}
	if(next_thread == NULL) {
		if(last_thread_pri != -1) {
			next_thread = thread_dequeue_run_q(last_thread_pri);
		} else {
			next_thread = thread_dequeue_run_q(THREAD_IDLE_PRIORITY);
		}
	}

	oldstate = next_thread->state;
	next_thread->state = THREAD_STATE_RUNNING;
	next_thread->next_state = THREAD_STATE_READY;

	if(next_thread != old_thread) {
		dprintf("thread_resched: switching from thread %d to %d\n",
			old_thread->id, next_thread->id);

		CURR_THREAD = next_thread;
		thread_context_switch(old_thread, next_thread);
#if 0
		if(oldstate == THREAD_STATE_NEW) {
			// this thread was not previously thread_resched()ed,
			// so it will not have the thread lock released after it is
			// started. Have the context switch code release the thread spinlock
			// after the switch is done.
			thread_context_switch(old_thread, next_thread, 1);
		} else {
			thread_context_switch(old_thread, next_thread, 0);
		}
#endif
	}
}
