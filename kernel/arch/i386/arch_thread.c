#include <string.h>
#include "debug.h"
#include "vm.h"
#include "thread.h"
#include "arch_thread.h"
#include "arch_cpu.h"

struct arch_thread *arch_thread_create_thread_struct()
{
	struct arch_thread *at;
	
	at = (struct arch_thread *)kmalloc(sizeof(struct arch_thread));
	if(at == NULL)
		return NULL;
	at->esp = NULL;
	
	return at;
}

int arch_thread_initialize_kthread_stack(struct thread *t, int (*start_func)(void), void (*entry_func)(void))
{
	unsigned int *kstack = (unsigned int *)t->kernel_stack_area->base;
	unsigned int kstack_size = t->kernel_stack_area->size;
	unsigned int *kstack_top = kstack + kstack_size / sizeof(unsigned int);
	struct arch_thread *at;
	int i;

//	dprintf("arch_thread_initialize_kthread_stack: kstack 0x%p, start_func 0x%p, entry_func 0x%p\n",
//		kstack, start_func, entry_func);

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

	// simulate pushfl
	kstack_top--;
	*kstack_top = 0x00; // interrupts still disabled after the switch

	// simulate initial popad
	for(i=0; i<8; i++) {
		kstack_top--;
		*kstack_top = 0;
	}

	// save the esp
	at = (struct arch_thread *)t->arch_info;
	at->esp = kstack_top;
	
	return 0;
}

void arch_thread_context_switch(struct thread *t_from, struct thread *t_to)
{
	struct arch_thread *old_at = (struct arch_thread *)t_from->arch_info;
	struct arch_thread *new_at = (struct arch_thread *)t_to->arch_info;
#if 0
	int i;

	dprintf("arch_thread_context_switch: &old_esp = 0x%p, esp = 0x%p\n",
		&old_at->esp, new_at->esp);

	for(i=0; i<11; i++)
		dprintf("*esp[%d] (0x%x) = 0x%x\n", i, ((unsigned int *)new_at->esp + i), *((unsigned int *)new_at->esp + i));
#endif
	i386_context_switch(&old_at->esp, new_at->esp);
}

void arch_thread_dump_info(void *info)
{
	struct arch_thread *at = (struct arch_thread *)info;

	dprintf("\tesp: 0x%x\n", at->esp);
}
