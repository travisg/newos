#include <string.h>
#include "debug.h"
#include "vm.h"
#include "thread.h"
#include "arch_thread.h"

struct arch_thread *arch_thread_create_thread_struct()
{
	struct arch_thread *at;
	
	at = (struct arch_thread *)kmalloc(sizeof(struct arch_thread));
	if(at == NULL)
		return NULL;
	at->esp = NULL;
	
	return at;
}

int arch_thread_initialize_kthread_stack(struct thread *t, int (*start_func)(void *param))
{
	unsigned int *kstack = (unsigned int *)t->kernel_stack_area->base;
	unsigned int kstack_size = t->kernel_stack_area->size;
	unsigned int *kstack_top = kstack + kstack_size / sizeof(unsigned int) - 1;
	struct arch_thread *at;
	int i;

//	dprintf("arch_thread_initialize_stack: kstack 0x%p, kstack_size %d, kstack_top 0x%p\n",
//		kstack, kstack_size, kstack_top);

	// clear the kernel stack
	memset(kstack, 0, kstack_size);

	// set the final return address to be thread_kthread_exit
	*kstack_top = (unsigned int)&thread_kthread_exit;
	kstack_top--;

	// set the return address to be the start of the first function
	*kstack_top = (unsigned int)start_func;
	kstack_top--;
	
	// simulate pushfl
	*kstack_top = 0x200;
	kstack_top--;
	
	// simulate initial popad
	for(i=0; i<7; i++) {
		*kstack_top = 0;
		kstack_top--;
	}
	
	// save the esp
	at = (struct arch_thread *)t->arch_info;
	at->esp = kstack_top;
	
	return 0;
}

void context_switch(unsigned int **old_esp, unsigned int *new_esp);
asm("
.globl context_switch	
context_switch:
	pushfl
	pusha
	movl	40(%esp),%eax
	movl	%esp,(%eax)
	movl	44(%esp),%eax
	movl	%eax,%esp
	popa
	popfl
	ret
");

void arch_thread_context_switch(struct thread *t_from, struct thread *t_to)
{
	struct arch_thread *old_at = (struct arch_thread *)t_from->arch_info;
	struct arch_thread *new_at = (struct arch_thread *)t_to->arch_info;

//	dprintf("arch_thread_context_switch: &old_esp = 0x%p, esp = 0x%p\n",
//		&old_at->esp, new_at->esp);

	context_switch(&old_at->esp, new_at->esp);
}

