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

static int next_thread_id = 0;
static int thread_spinlock = 0;

#define CURR_THREAD cur_thread[smp_get_current_cpu()]

static struct thread **cur_thread = NULL;
static struct thread *thread_list = NULL;

// thread queues
static struct thread *run_q_head[THREAD_NUM_PRIORITY_LEVELS] = { NULL, };
static struct thread *run_q_tail[THREAD_NUM_PRIORITY_LEVELS] = { NULL, };

// insert a thread onto the tail of a queue	
static void _enqueue_thread(struct thread *t, struct thread **q_head, struct thread **q_tail)
{
	t->q_next = NULL;
	if(*q_head == NULL) {
		*q_head = t;
		*q_tail = t;
	} else {
		(*q_tail)->q_next = t;
		*q_tail = t;
	}
}

static struct thread *_lookat_queue_thread(struct thread **q_head, struct thread **q_tail)
{
	return *q_head;
}

static struct thread *_dequeue_thread(struct thread **q_head, struct thread **q_tail)
{
	struct thread *t;	

	t = *q_head;
	if(t != NULL) {
		*q_head = t->q_next;
		if(*q_tail == t)
			*q_tail = NULL;
	}
	return t;
}

static void enqueue_run_q(struct thread *t)
{
	// these shouldn't exist
	if(t->priority > THREAD_MAX_PRIORITY)
		t->priority = THREAD_MAX_PRIORITY;
	if(t->priority < 0)
		t->priority = 0;

	_enqueue_thread(t, &run_q_head[t->priority], &run_q_tail[t->priority]);
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
	t->state = THREAD_STATE_RUN;

	sprintf(stack_name, "%s_kstack", name);
	vm_create_area(t->proc->aspace, stack_name, (void **)&kstack_addr,
		AREA_ANY_ADDRESS, KSTACK_SIZE, 0);
	t->kernel_stack_area = vm_find_area_by_name(t->proc->aspace, stack_name);
	arch_thread_initialize_kthread_stack(t, func);
	
	insert_thread_into_proc(t->proc, t);
	
	return t;
}

int thread_init(struct kernel_args *ka)
{
	struct thread *t;
	int i;
	
	dprintf("thread_init: entry\n");

	// zero out the run queues
	memset(run_q_head, 0, sizeof(run_q_head));
	memset(run_q_tail, 0, sizeof(run_q_tail));

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
		t->state = THREAD_STATE_RUN;
		sprintf(temp, "idle_thread%d_kstack", i);
		t->kernel_stack_area = vm_find_area_by_name(t->proc->aspace, temp);		
		insert_thread_into_proc(t->proc, t);
		cur_thread[i] = t;
	}

	return 0;
}

int thread_kthread_exit()
{
	dprintf("kernel thread exiting\n");
	
	// XXX resched
	for(;;);
}

int thread_get_current_id()
{
	return CURR_THREAD->id;
}

static void thread_context_switch(struct thread *t_from, struct thread *t_to)
{
	arch_thread_context_switch(t_from, t_to);
}

/* thread TEST code */
int test_thread(void *unused)
{
	int a = 0;
	int tid = thread_get_current_id();
	
	while(1) {
//		a += tid;
		a++;
		kprintf_xy(0, tid-1, "thread%d - %d    - cpu %d", tid, a, smp_get_current_cpu());
	}
	return 1;
}

int thread_test()
{
	struct thread *t;
	// create a few threads
	t = create_kernel_thread("test_thread1", &test_thread, 5);
	enqueue_run_q(t);
	t = create_kernel_thread("test_thread2", &test_thread, 10);
	enqueue_run_q(t);
	t = create_kernel_thread("test_thread3", &test_thread, 12);
	enqueue_run_q(t);
	t = create_kernel_thread("test_thread4", &test_thread, 20);
	enqueue_run_q(t);
	t = create_kernel_thread("test_thread5", &test_thread, 25);
	enqueue_run_q(t);
	t = create_kernel_thread("test_thread6", &test_thread, 29);
	enqueue_run_q(t);
	t = create_kernel_thread("test_thread7", &test_thread, 36);
	enqueue_run_q(t);
	t = create_kernel_thread("test_thread8", &test_thread, 64);
	enqueue_run_q(t);
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

int thread_resched()
{
	struct thread *next_thread = NULL;
	int last_thread_pri = -1;
	struct thread *old_thread = CURR_THREAD;
	int i;
	
	acquire_spinlock(&thread_spinlock);
	
	if(old_thread->state == THREAD_STATE_RUN)
		enqueue_run_q(old_thread);

	for(i = THREAD_MAX_PRIORITY; i > THREAD_IDLE_PRIORITY; i--) {
		next_thread = _lookat_queue_thread(&run_q_head[i], &run_q_tail[i]);
		if(next_thread != NULL) {
			// skip it sometimes
			if(_rand() > 0x3000) {
				next_thread = _dequeue_thread(&run_q_head[i], &run_q_tail[i]);
				break;
			}
			last_thread_pri = i;
			next_thread = NULL;
		}
	}
	if(next_thread == NULL) {
		if(last_thread_pri != -1) {
			next_thread = _dequeue_thread(&run_q_head[last_thread_pri],
				&run_q_tail[last_thread_pri]);
		} else {
			next_thread = _dequeue_thread(&run_q_head[THREAD_IDLE_PRIORITY],
				&run_q_tail[THREAD_IDLE_PRIORITY]);
		}
	}

	if(next_thread != old_thread) {
//		dprintf("thread_resched: switching from thread %d to %d\n",
//			old_thread->id, next_thread->id);

		CURR_THREAD = next_thread;
		release_spinlock(&thread_spinlock);
		thread_context_switch(old_thread, next_thread);
	} else {
//		dprintf("thread_resched: keeping old thread %d\n", old_thread->id);
		release_spinlock(&thread_spinlock);
	}
	
	return 0;
}
