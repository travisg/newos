#include <kernel.h>
#include <thread.h>

int arch_proc_init_proc_struct(struct proc *p, bool kernel)
{
	return 0;
}

int arch_thread_init_thread_struct(struct thread *t)
{
	return 0;
}

int arch_thread_initialize_kthread_stack(struct thread *t, int (*start_func)(void), void (*entry_func)(void))
{
	return 0;
}

void arch_thread_context_switch(struct thread *t_from, struct thread *t_to)
{
	return;
}

void arch_thread_dump_info(void *info)
{
	return;
}

