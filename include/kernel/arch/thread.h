/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_THREAD_H
#define _NEWOS_KERNEL_ARCH_THREAD_H

#include <signal.h>

// forward declarations to make the compiler happy and keep the header
// interdependencies down
struct thread;
struct vm_translation_map_struct;

int arch_thread_init(kernel_args *ka);

int arch_proc_init_proc_struct(struct proc *p, bool kernel);
int arch_thread_init_thread_struct(struct thread *t);
void arch_thread_context_switch(struct thread *t_from, struct thread *t_to, struct vm_translation_map_struct *new_tmap);
int arch_thread_initialize_kthread_stack(struct thread *t, int (*start_func)(void));
void arch_thread_dump_info(void *info);
void arch_thread_enter_uspace(struct thread *t, addr_t entry, void *args, addr_t ustack_top);
void arch_thread_switch_kstack_and_call(addr_t new_kstack, void (*func)(void *), void *arg);

static struct thread *arch_thread_get_current_thread(void);
static void arch_thread_set_current_thread(struct thread *t);

int arch_setup_signal_frame(struct thread *t, struct sigaction *sa, int sig, int sig_mask);
int64 arch_restore_signal_frame(void);
void arch_check_syscall_restart(struct thread *t);

// for any inline overrides
#include INC_ARCH(kernel/arch,thread.h)

#endif

