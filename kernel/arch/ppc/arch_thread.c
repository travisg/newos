/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <boot/stage2.h>
#include <string.h>

int arch_proc_init_proc_struct(struct proc *p, bool kernel)
{
	return 0;
}

int arch_thread_init_thread_struct(struct thread *t)
{
	// set up an initial state (stack & fpu)
	memset(&t->arch_info, 0, sizeof(t->arch_info));

	return 0;
}

int arch_thread_initialize_kthread_stack(struct thread *t, int (*start_func)(void))
{
	addr_t *kstack = (addr_t *)t->kernel_stack_base;
	size_t kstack_size = KSTACK_SIZE;
	addr_t *kstack_top = kstack + kstack_size / sizeof(addr_t) - 2 * 4;

	//	dprintf("arch_thread_initialize_kthread_stack: kstack 0x%p, start_func %p\n", kstack, start_func);

	// r13-r31, cr, r2
	kstack_top -= (31 - 13 + 1) + 1 + 1;

	// set the saved lr address to be the start_func
	kstack_top--;
	*kstack_top = (addr_t)start_func;

	// save this stack position
	t->arch_info.sp = (addr_t)kstack_top;

	return 0;
}

void arch_thread_dump_info(void *info)
{
	struct arch_thread *at = (struct arch_thread *)info;

	dprintf("\tsp: 0x%lx\n", at->sp);
}

void arch_thread_enter_uspace(struct thread *t, addr_t entry, void *args, addr_t ustack_top)
{
	PANIC_UNIMPLEMENTED();
}

void arch_thread_context_switch(struct thread *t_from, struct thread *t_to)
{
#if 0
	dprintf("arch_thread_context_switch: cpu %d 0x%x (%s) -> 0x%x (%s), aspace %p -> %p, old stack = 0x%lx, new stack = 0x%lx\n",
		smp_get_current_cpu(),
		t_from->id, t_from->name,
		t_to->id, t_to->name,
		t_from->proc->aspace, t_to->proc->aspace,
		t_from->arch_info.sp,
		t_to->arch_info.sp);
#endif

	// set the new kernel stack in the EAR register.
	// this is used in the exception handler code to decide what kernel stack to switch
	//  to if the exception had happened when the processor was in user mode
	asm("mtear	%0" :: "g"(t_to->kernel_stack_base + KSTACK_SIZE - 8));

	// switch the asids if we need to
	if(t_to->proc->aspace_id >= 0) {
		// the target thread has is user space
		if(t_from->proc->aspace_id != t_to->proc->aspace_id) {
			// switching to a new address space
			vm_translation_map_change_asid(&t_to->proc->aspace->translation_map);
		}
	}

#if 0
{
	int i;

	dprintf("top of stack:\n");
	for(i=0; i < 22; i++) {
		dprintf("0x%lx: 0x%x\n", (t_to->arch_info.sp + i*4), *(unsigned int *)(t_to->arch_info.sp + i*4));
	}
}
#endif

	ppc_context_switch(&t_from->arch_info.sp, t_to->arch_info.sp);
}

// XXX we are single proc for now
static struct thread *current_thread = NULL;

struct thread *arch_thread_get_current_thread(void)
{
	return current_thread;
}

void arch_thread_set_current_thread(struct thread *t)
{
	current_thread = t;
}

