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

// thread that wakes up manny
static int killer_sem = -1;

// thread queues
static struct thread_queue run_q[THREAD_NUM_PRIORITY_LEVELS] = { { NULL, NULL }, };
static struct thread_queue death_q = { NULL, NULL };

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
	
	t = (struct thread *)kmalloc(sizeof(struct thread));
	if(t == NULL)
		return NULL;
	t->name = (char *)kmalloc(strlen(name) + 1);
	if(t->name == NULL) {
		kfree(t);
		return NULL;
	}
	strcpy(t->name, name);
	t->id = next_thread_id++;
	t->proc = NULL;
	t->kernel_stack_area = NULL;
	t->user_stack_area = NULL;
	t->proc_next = NULL;
	t->q_next = NULL;
	t->priority = -1;
	t->arch_info = arch_thread_create_thread_struct();
	if(t->arch_info == NULL) {
		kfree(t->name);
		kfree(t);
		return NULL;
	}
	// insert into global list
	t->all_next = thread_list;
	thread_list = t;
	
	return t;
}

static struct thread *create_kernel_thread(char *name, int (*func)(void *param), int priority)
{
	struct thread *t;
	unsigned int *kstack_addr;
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
	arch_thread_initialize_kthread_stack(t, func);
	
	insert_thread_into_proc(t->proc, t);
	
	return t;
}

int thread_killer(void *unused)
{
	killer_sem = sem_create(0, "killer_sem");

	while(1) {
		int state;
		
		sem_acquire(killer_sem, 1);

		// kill something
		state = int_disable_interrupts();
		GRAB_THREAD_LOCK;
		
		// XXX need snooze to do this right
		
		RELEASE_THREAD_LOCK;
		int_restore_interrupts(state);			
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
		insert_thread_into_proc(t->proc, t);
		cur_thread[i] = t;
	}

	// create a worker thread to kill other ones
	t = create_kernel_thread("manny", &thread_killer, THREAD_MAX_PRIORITY);
	thread_enqueue_run_q(t);

	return 0;
}

int thread_kthread_exit()
{
#if 0 // need to have snooze() to do this right
	int state;
	struct thread *t = CURR_THREAD;
	
	dprintf("kernel thread exiting\n");
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK;
	
	// put ourselves into the death queue
	thread_enqueue(t, &death_q);
	
	RELEASE_THREAD_LOCK;
	
	sem_release_etc(killer_sem, 1, SEM_FLAG_NO_RESCHED);

	GRAB_THREAD_LOCK;
	t->next_state = THREAD_STATE_MARKED_FOR_DEATH;
	RELEASE_THREAD_LOCK;
	int_restore_interrupts(state);
#endif	
	// XXX resched
	for(;;);
}

int thread_get_current_id()
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

int test_thread(void *unused)
{
	int a = 0;
	int tid = thread_get_current_id();
	
	while(1) {
//		a += tid;
		a++;
		kprintf_xy(0, tid-1, "thread%d - %d    - cpu %d", tid, a, smp_get_current_cpu());
		switch(tid) {
			case 3: case 6:
				if((a % 2048) == 0)
					sem_release(thread_test_sem, 1);
				break;
			default:
				sem_acquire(thread_test_sem, 1);
		}
	}
	return 1;
}

int thread_test()
{
	struct thread *t;
	// create a few threads
	t = create_kernel_thread("test_thread1", &test_thread, 5);
	thread_enqueue_run_q(t);
	t = create_kernel_thread("test_thread2", &test_thread, 10);
	thread_enqueue_run_q(t);
	t = create_kernel_thread("test_thread3", &test_thread, 12);
	thread_enqueue_run_q(t);
	t = create_kernel_thread("test_thread4", &test_thread, 20);
	thread_enqueue_run_q(t);
	t = create_kernel_thread("test_thread5", &test_thread, 25);
	thread_enqueue_run_q(t);
	t = create_kernel_thread("test_thread6", &test_thread, 29);
	thread_enqueue_run_q(t);
	t = create_kernel_thread("test_thread7", &test_thread, 36);
	thread_enqueue_run_q(t);
	t = create_kernel_thread("test_thread8", &test_thread, 64);
	thread_enqueue_run_q(t);

	thread_test_sem = sem_create(1, "test");
	return 0;
}

static int
_rand()
{
	// XXX better seed
	static int next = 871223;

	next = next * 1103515245 + 12345;
	return((next >> 16) & 0x7FFF);
}

// NOTE: expects thread_spinlock to be held
int thread_resched()
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
//		dprintf("thread_resched: switching from thread %d to %d\n",
//			old_thread->id, next_thread->id);

		CURR_THREAD = next_thread;
		if(oldstate == THREAD_STATE_NEW) {
			// this thread was not previously thread_resched()ed,
			// so it will not have the thread lock released after it is
			// started. We'll release it ahead of time.
			RELEASE_THREAD_LOCK;
		}
		thread_context_switch(old_thread, next_thread);
	}
	
	return 0;
}
