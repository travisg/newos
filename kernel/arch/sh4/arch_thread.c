#include <kernel.h>
#include <thread.h>
#include <string.h>
#include <debug.h>
#include <arch_cpu.h>

int arch_proc_init_proc_struct(struct proc *p, bool kernel)
{
	// XXX finish
	return 0;
}

int arch_thread_init_thread_struct(struct thread *t)
{
	t->arch_info.sp = NULL;

	return 0;
}

int arch_thread_initialize_kthread_stack(struct thread *t, int (*start_func)(void), void (*entry_func)(void))
{
	unsigned int *kstack = (unsigned int *)t->kernel_stack_area->base;
	unsigned int kstack_size = t->kernel_stack_area->size;
	unsigned int *kstack_top = kstack + kstack_size / sizeof(unsigned int);
	int i;

	// clear the kernel stack
	memset(kstack, 0, kstack_size);

	// set the final return address to be thread_kthread_exit
	kstack_top--;
	*kstack_top = (unsigned int)&thread_kthread_exit;

	// set the return address to be the start of the first function
	kstack_top--;
	*kstack_top = (unsigned int)start_func;

	// set the return address to be the start of the entry (thread setup) function
	kstack_top--;
	*kstack_top = (unsigned int)entry_func;

	// simulate the important registers being pushed
	for(i=0; i<7; i++) {
		kstack_top--;
		*kstack_top = 0;
	}

	// set the address of the function caller function
	// that will in turn call all of the above functions
	// pushed onto the stack
	kstack_top--;
	*kstack_top = (unsigned int)sh4_function_caller;

	// save the sp
	t->arch_info.sp = kstack_top;

	return 0;
}

void arch_thread_context_switch(struct thread *t_from, struct thread *t_to)
{
#if 0
	int i;
	dprintf("arch_thread_context_switch: to sp 0x%x\n", t_to->arch_info.sp);
	for(i=0; i<8; i++) {
		dprintf("sp[%d] = 0x%x\n", i, t_to->arch_info.sp[i]);
	}
#endif
	sh4_context_switch(&t_from->arch_info.sp, t_to->arch_info.sp);
}

void arch_thread_dump_info(void *info)
{
	struct arch_thread *at = (struct arch_thread *)info;
	dprintf("\tsp: 0x%x\n", at->sp);
}

