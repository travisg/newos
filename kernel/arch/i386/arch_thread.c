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

// from arch_interrupts.S
extern void	i386_stack_init( struct farcall *interrupt_stack_offset );
extern void i386_return_from_signal(void);
extern void i386_end_return_from_signal(void);

int arch_thread_init(kernel_args *ka)
{
	return 0;
}

int arch_proc_init_proc_struct(struct proc *p, bool kernel)
{
	return 0;
}

void i386_push_iframe(struct iframe *frame)
{
	struct thread *t = thread_get_current_thread();
	ASSERT(t->arch_info.iframe_ptr < IFRAME_TRACE_DEPTH);

//	dprintf("i386_push_iframe: frame %p, depth %d\n", frame, t->arch_info.iframe_ptr);
	
	t->arch_info.iframes[t->arch_info.iframe_ptr++] = frame;
}

void i386_pop_iframe(void)
{
	struct thread *t = thread_get_current_thread();
	ASSERT(t->arch_info.iframe_ptr > 0);
	t->arch_info.iframe_ptr--;
}

struct iframe *i386_get_curr_iframe(void)
{
	struct thread *t = thread_get_current_thread();
	ASSERT(t->arch_info.iframe_ptr >= 0);
	return t->arch_info.iframes[t->arch_info.iframe_ptr-1];
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

void arch_thread_context_switch(struct thread *t_from, struct thread *t_to, struct vm_translation_map_struct *new_tmap)
{
#if 0
	int i;

	dprintf("arch_thread_context_switch: cpu %d 0x%x -> 0x%x, aspace 0x%x -> 0x%x, old stack = 0x%x:0x%x, stack = 0x%x:0x%x\n",
		smp_get_current_cpu(), t_from->id, t_to->id,
		t_from->proc->aspace, t_to->proc->aspace,
		t_from->arch_info.current_stack.ss, t_from->arch_info.current_stack.esp,
		t_to->arch_info.current_stack.ss, t_to->arch_info.current_stack.esp);
#endif

	// if we're switching to a new translation map, look up the page directory
	// and switch to it.
	if(new_tmap) {
		addr_t new_pgdir = vm_translation_map_get_pgdir(new_tmap);

		if((new_pgdir % PAGE_SIZE) != 0)
			panic("arch_thread_context_switch: bad pgdir %p\n", (void*)new_pgdir);

#if 0
		dprintf("new_pgdir is 0x%x\n", new_pgdir);
#endif

		i386_swap_pgdir(new_pgdir);
	}

	i386_set_task_switched(); // disables the fpu
	i386_set_kstack(t_to->kernel_stack_base + KSTACK_SIZE);
	i386_context_switch(&t_from->arch_info, &t_to->arch_info);
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

int
arch_setup_signal_frame(struct thread *t, struct sigaction *sa, int sig, int sig_mask)
{
	struct iframe *frame = i386_get_curr_iframe();
	uint32 *stack_ptr;
	uint32 *code_ptr;
	uint32 *regs_ptr;
	struct vregs regs;
	uint32 stack_buf[6];
	int err;

	/* do some quick sanity checks */
	ASSERT(frame);
	ASSERT(is_user_address(frame->user_esp));

//	dprintf("arch_setup_signal_frame: thread 0x%x, frame %p, user_esp 0x%x, sig %d\n",
//		t->id, frame, frame->user_esp, sig);

	if ((int)frame->orig_eax >= 0) {
		// we're coming from a syscall
		if (((int)frame->eax == ERR_INTERRUPTED) && (sa->sa_flags & SA_RESTART)) {
			dprintf("### restarting syscall %d after signal %d\n", frame->orig_eax, sig);
			frame->eax = frame->orig_eax;
			frame->edx = frame->orig_edx;
			frame->eip -= 2;
		}
	}

	// start stuffing stuff on the user stack
	stack_ptr = (uint32 *)frame->user_esp;

	// store the saved regs onto the user stack
	stack_ptr -= ROUNDUP(sizeof(struct vregs)/4, 4);
	regs_ptr = stack_ptr;
	regs.eip = frame->eip;
	regs.eflags = frame->flags;
	regs.eax = frame->eax;
	regs.ecx = frame->ecx;
	regs.edx = frame->edx;
	regs.esp = frame->esp;
	regs._reserved_1 = frame->user_esp;
	regs._reserved_2[0] = frame->edi;
	regs._reserved_2[1] = frame->esi;
	regs._reserved_2[2] = frame->ebp;
	i386_fsave((void *)(&regs.xregs));
	
	err = user_memcpy(stack_ptr, &regs, sizeof(regs));
	if(err < 0)
		return err;

	// now store a code snippet on the stack
	stack_ptr -= ((uint32)i386_end_return_from_signal - (uint32)i386_return_from_signal)/4;
	code_ptr = stack_ptr;
	err = user_memcpy(code_ptr, i386_return_from_signal,
		((uint32)i386_end_return_from_signal - (uint32)i386_return_from_signal));
	if(err < 0)
		return err;

	// now set up the final part
	stack_buf[0] = (uint32)code_ptr;	// return address when sa_handler done
	stack_buf[1] = sig;					// first argument to sa_handler
	stack_buf[2] = (uint32)sa->sa_userdata;// second argument to sa_handler
	stack_buf[3] = (uint32)regs_ptr;	// third argument to sa_handler

	stack_buf[4] = sig_mask;			// Old signal mask to restore
	stack_buf[5] = (uint32)regs_ptr;	// Int frame + extra regs to restore

	stack_ptr -= sizeof(stack_buf)/4;

	err = user_memcpy(stack_ptr, stack_buf, sizeof(stack_buf));
	if(err < 0)
		return err;
	
	frame->user_esp = (uint32)stack_ptr;
	frame->eip = (uint32)sa->sa_handler;

	return NO_ERROR;
}


int64
arch_restore_signal_frame(void)
{
	struct thread *t = thread_get_current_thread();
	struct iframe *frame;
	uint32 *stack;
	struct vregs *regs;
	
//	dprintf("### arch_restore_signal_frame: entry\n");
	
	frame = i386_get_curr_iframe();
	
	stack = (uint32 *)frame->user_esp;
	t->sig_block_mask = stack[0];
	regs = (struct vregs *)stack[1];

	frame->eip = regs->eip;
	frame->flags = regs->eflags;
	frame->eax = regs->eax;
	frame->ecx = regs->ecx;
	frame->edx = regs->edx;
	frame->esp = regs->esp;
	frame->user_esp = regs->_reserved_1;
	frame->edi = regs->_reserved_2[0];
	frame->esi = regs->_reserved_2[1];
	frame->ebp = regs->_reserved_2[2];
	
	i386_frstor((void *)(&regs->xregs));
	
//	dprintf("### arch_restore_signal_frame: exit\n");
	
	frame->orig_eax = -1;	/* disable syscall checks */
	
	return (int64)frame->eax | ((int64)frame->edx << 32);
}


void
arch_check_syscall_restart(struct thread *t)
{
	struct iframe *frame = i386_get_curr_iframe();

	if(!frame)
		return;

	if (((int)frame->orig_eax >= 0) && ((int)frame->eax == ERR_INTERRUPTED)) {
		frame->eax = frame->orig_eax;
		frame->edx = frame->orig_edx;
		frame->eip -= 2;
	}
}

