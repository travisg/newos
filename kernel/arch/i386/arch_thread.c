/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/debug.h>
#include <kernel/vm.h>
#include <kernel/heap.h>
#include <kernel/thread.h>
#include <kernel/arch/thread.h>
#include <kernel/int.h>
#include <string.h>

int arch_proc_init_proc_struct(struct proc *p, bool kernel)
{
	return 0;
}

// from arch_interrupts.S
extern void	i386_stack_init( struct farcall *interrupt_stack_offset );

void i386_push_iframe(struct thread *t, struct iframe *frame)
{
	ASSERT(t->arch_info.iframe_ptr < IFRAME_TRACE_DEPTH);
	t->arch_info.iframes[t->arch_info.iframe_ptr++] = frame;
}

void i386_pop_iframe(struct thread *t)
{
	ASSERT(t->arch_info.iframe_ptr > 0);
	t->arch_info.iframe_ptr--;
}

int arch_thread_init_thread_struct(struct thread *t)
{
	// set up an initial state (stack & fpu)
	memset(&t->arch_info, 0, sizeof(t->arch_info));

	// let the asm function know the offset to the interrupt stack within struct thread
	// I know no better ( = static) way to tell the asm function the offset
	i386_stack_init( &((struct thread*)0)->arch_info.interrupt_stack );

	return 0;
}

int arch_thread_initialize_kthread_stack(struct thread *t, int (*start_func)(void))
{
	unsigned int *kstack = (unsigned int *)t->kernel_stack_base;
	unsigned int kstack_size = KSTACK_SIZE;
	unsigned int *kstack_top = kstack + kstack_size / sizeof(unsigned int);
	int i;

//	dprintf("arch_thread_initialize_kthread_stack: kstack 0x%p, start_func %p\n", kstack, start_func);

	// set the return address to be the start of the first function
	kstack_top--;
	*kstack_top = (unsigned int)start_func;

	// simulate initial popad
	for(i=0; i<8; i++) {
		kstack_top--;
		*kstack_top = 0;
	}

	// save the stack position
	t->arch_info.current_stack.esp = kstack_top;
	t->arch_info.current_stack.ss = (int *)KERNEL_DATA_SEG;

	return 0;
}

void arch_thread_switch_kstack_and_call(addr_t new_kstack, void (*func)(void *), void *arg)
{
	i386_switch_stack_and_call(new_kstack, func, arg);
}

void arch_thread_context_switch(struct thread *t_from, struct thread *t_to)
{
	addr_t new_pgdir;
#if 0
	int i;

	dprintf("arch_thread_context_switch: cpu %d 0x%x -> 0x%x, aspace 0x%x -> 0x%x, old stack = 0x%x:0x%x, stack = 0x%x:0x%x\n",
		smp_get_current_cpu(), t_from->id, t_to->id,
		t_from->proc->aspace, t_to->proc->aspace,
		t_from->arch_info.current_stack.ss, t_from->arch_info.current_stack.esp,
		t_to->arch_info.current_stack.ss, t_to->arch_info.current_stack.esp);
#endif
#if 0
	for(i=0; i<11; i++)
		dprintf("*esp[%d] (0x%x) = 0x%x\n", i, ((unsigned int *)new_at->esp + i), *((unsigned int *)new_at->esp + i));
#endif
	i386_set_kstack(t_to->kernel_stack_base + KSTACK_SIZE);

#if 0
{
	int a = *(int *)(t_to->kernel_stack_base + KSTACK_SIZE - 4);
}
#endif

	if(t_from->proc->aspace_id >= 0 && t_to->proc->aspace_id >= 0) {
		// they are both uspace threads
		if(t_from->proc->aspace_id == t_to->proc->aspace_id) {
			// dont change the pgdir, same address space
			new_pgdir = NULL;
		} else {
			// switching to a new address space
			new_pgdir = vm_translation_map_get_pgdir(&t_to->proc->aspace->translation_map);
		}
	} else if(t_from->proc->aspace_id < 0 && t_to->proc->aspace_id < 0) {
		// they must both be kspace threads
		new_pgdir = NULL;
	} else if(t_to->proc->aspace_id < 0) {
		// the one we're switching to is kspace
		new_pgdir = vm_translation_map_get_pgdir(&t_to->proc->kaspace->translation_map);
	} else {
		new_pgdir = vm_translation_map_get_pgdir(&t_to->proc->aspace->translation_map);
	}
#if 0
	dprintf("new_pgdir is 0x%x\n", new_pgdir);
#endif

#if 0
{
	int a = *(int *)(t_to->arch_info.current_stack.esp - 4);
}
#endif

	if((new_pgdir % PAGE_SIZE) != 0)
		panic("arch_thread_context_switch: bad pgdir %p\n", (void*)new_pgdir);

	i386_fsave_swap(t_from->arch_info.fpu_state, t_to->arch_info.fpu_state);
	i386_context_switch(&t_from->arch_info, &t_to->arch_info, new_pgdir);
}

void arch_thread_dump_info(void *info)
{
	struct arch_thread *at = (struct arch_thread *)info;

	dprintf("\tesp: %p\n", at->current_stack.esp);
	dprintf("\tss: %p\n", at->current_stack.ss);
	dprintf("\tfpu_state at %p\n", at->fpu_state);
}

void arch_thread_enter_uspace(struct thread *t, addr_t entry, void *args, addr_t ustack_top)
{
	dprintf("arch_thread_entry_uspace: thread 0x%x, entry 0x%lx, args %p, ustack_top 0x%lx\n",
		t->id, entry, args, ustack_top);

	// make sure the fpu is in a good state
	asm("fninit");

	/*
	 * semi-hack: touch the user space stack first to page it in before we
	 * disable interrupts. We can't take a fault with interrupts off
	 */
	{
		int a = *(volatile int *)(ustack_top - 4);
		TOUCH(a);
	}

	int_disable_interrupts();

	i386_set_kstack(t->kernel_stack_base + KSTACK_SIZE);

	// set the interrupt disable count to zero, since we'll have ints enabled as soon as we enter user space
	t->int_disable_level = 0;

	i386_enter_uspace(entry, args, ustack_top - 4);
}

