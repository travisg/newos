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

static int next_thread_id = 0;

#define CUR_THREAD cur_thread[smp_get_current_cpu()]
static struct thread *cur_thread[MAX_CPUS] = { NULL, };

static struct thread *thread_list = NULL;

static void insert_thread_into_proc(struct proc *p, struct thread *t)
{
	t->next = p->thread_list;
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

static struct thread *create_kernel_thread(char *name, int (*func)(void *param))
{
	struct thread *t;
	unsigned int *kstack_addr;
	char stack_name[64];
	
	t = create_thread_struct(name);
	if(t == NULL)
		return NULL;
	t->proc = proc_get_kernel_proc();

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
	
	dprintf("thread_init: entry\n");

	// create thread 0 (idle thread)
	t = create_thread_struct("idle_thread");
	t->proc = proc_get_kernel_proc();
	t->kernel_stack_area = vm_find_area_by_name(t->proc->aspace, "idle_thread_kstack");		
	insert_thread_into_proc(t->proc, t);

	// set current thread
	CUR_THREAD = t;

#if 0
	// XXX remove
	idle_thread = t;
#endif
	return 0;
}

int thread_kthread_exit()
{
	dprintf("kernel thread exiting\n");
	
	// XXX resched
	for(;;);
}

static void thread_context_switch(struct thread *t_from, struct thread *t_to)
{
	arch_thread_context_switch(t_from, t_to);
}

/* thread TEST code */
struct thread *t1 = NULL;
struct thread *t2 = NULL;

int thread1_func(void *unused)
{
	int a = 0;
	
	while(1) {
//		dprintf("thread1\n");
		kprintf_xy(0, 0, "thread1 - %d", a++);
//		arch_thread_context_switch(t1, t2);
	}
	return 1;
}

int thread2_func(void *unused)
{
	int a = 0;
	
	while(1) {
//		dprintf("thread2\n");
		kprintf_xy(0, 1, "thread2 - %d", a--);
//		arch_thread_context_switch(t2, t1);
	}
	return 2;
}

int thread_test()
{
	// create a pair of threads
	t1 = create_kernel_thread("test_thread1", &thread1_func);
	t2 = create_kernel_thread("test_thread2", &thread2_func);

	int_enable_interrupts();

//	arch_thread_context_switch(idle_thread, t1);

	return 0;
}

int thread_resched()
{
	// XXX thread test
	{
		struct thread *switch_to;
		
		if(CUR_THREAD == t1) {
			switch_to = t2;
		} else {
			switch_to = t1;
		}
//		dprintf("thread_resched: switching from thread %d to %d\n",
//			CUR_THREAD->id, switch_to->id);
		CUR_THREAD = switch_to;
		thread_context_switch(CUR_THREAD, switch_to);
	}
	return 0;
}
