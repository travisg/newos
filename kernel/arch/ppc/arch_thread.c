/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/vm.h>
#include <kernel/arch/vm_translation_map.h>
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

    //  dprintf("arch_thread_initialize_kthread_stack: kstack 0x%p, start_func %p\n", kstack, start_func);

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
    if (t_to->proc->aspace_id >= 0) {
        // the target thread has is user space
        if (t_from->proc->aspace_id != t_to->proc->aspace_id) {
            // switching to a new address space
            vm_translation_map_change_asid(&t_to->proc->aspace->translation_map);
        }
    }

#if 0
    {
        int i;

        dprintf("top of stack:\n");
        for (i=0; i < 22; i++) {
            dprintf("0x%lx: 0x%x\n", (t_to->arch_info.sp + i*4), *(unsigned int *)(t_to->arch_info.sp + i*4));
        }
    }
#endif

    ppc_context_switch(&t_from->arch_info.sp, t_to->arch_info.sp);
}

// XXX we are single proc for now
static struct thread *current_thread = NULL;

// XXX can we find a spare register to stuff this in?

struct thread *arch_thread_get_current_thread(void)
{
    return current_thread;
}

void arch_thread_set_current_thread(struct thread *t)
{
    current_thread = t;
}

int
arch_setup_signal_frame(struct thread *t, struct sigaction *sa, int sig, int sig_mask)
{
    PANIC_UNIMPLEMENTED();
#if 0
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

//  dprintf("arch_setup_signal_frame: thread 0x%x, frame %p, user_esp 0x%x, sig %d\n",
//      t->id, frame, frame->user_esp, sig);

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
    if (err < 0)
        return err;

    // now store a code snippet on the stack
    stack_ptr -= ((uint32)i386_end_return_from_signal - (uint32)i386_return_from_signal)/4;
    code_ptr = stack_ptr;
    err = user_memcpy(code_ptr, i386_return_from_signal,
                      ((uint32)i386_end_return_from_signal - (uint32)i386_return_from_signal));
    if (err < 0)
        return err;

    // now set up the final part
    stack_buf[0] = (uint32)code_ptr;    // return address when sa_handler done
    stack_buf[1] = sig;                 // first argument to sa_handler
    stack_buf[2] = (uint32)sa->sa_userdata;// second argument to sa_handler
    stack_buf[3] = (uint32)regs_ptr;    // third argument to sa_handler

    stack_buf[4] = sig_mask;            // Old signal mask to restore
    stack_buf[5] = (uint32)regs_ptr;    // Int frame + extra regs to restore

    stack_ptr -= sizeof(stack_buf)/4;

    err = user_memcpy(stack_ptr, stack_buf, sizeof(stack_buf));
    if (err < 0)
        return err;

    frame->user_esp = (uint32)stack_ptr;
    frame->eip = (uint32)sa->sa_handler;

    return NO_ERROR;
#endif
}


int64
arch_restore_signal_frame(void)
{
    PANIC_UNIMPLEMENTED();
#if 0
    struct thread *t = thread_get_current_thread();
    struct iframe *frame;
    uint32 *stack;
    struct vregs *regs;

//  dprintf("### arch_restore_signal_frame: entry\n");

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

//  dprintf("### arch_restore_signal_frame: exit\n");

    frame->orig_eax = -1;   /* disable syscall checks */

    return (int64)frame->eax | ((int64)frame->edx << 32);
#endif
}


void
arch_check_syscall_restart(struct thread *t)
{
    PANIC_UNIMPLEMENTED();
#if 0
    struct iframe *frame = i386_get_curr_iframe();

    if (!frame)
        return;

    if (((int)frame->orig_eax >= 0) && ((int)frame->eax == ERR_INTERRUPTED)) {
        frame->eax = frame->orig_eax;
        frame->edx = frame->orig_edx;
        frame->eip -= 2;
    }
#endif
}


