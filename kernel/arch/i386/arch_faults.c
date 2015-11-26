/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/faults.h>
#include <kernel/faults_priv.h>
#include <kernel/vm.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/int.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>
#include <kernel/arch/faults.h>

#include <kernel/arch/i386/interrupts.h>
#include <kernel/arch/i386/faults.h>

#include <boot/stage2.h>

#include <string.h>

// XXX this module is largely outdated. Will probably be removed later.

int arch_faults_init(kernel_args *ka)
{
    return 0;
}

int i386_general_protection_fault(int errorcode)
{
    return general_protection_fault(errorcode);
}

int i386_double_fault(int errorcode)
{
    kprintf("double fault! errorcode = 0x%x\n", errorcode);
    dprintf("double fault! errorcode = 0x%x\n", errorcode);
    for (;;);
    return INT_NO_RESCHEDULE;
}

int i386_device_not_available(void)
{
    cpu_ent *cpu = get_curr_cpu_struct();
    struct thread *t = thread_get_current_thread();

//  dprintf("***device_not_available: cpu %d holds fpu context for thread %d. current thread %d, state on cpu %d\n",
//      cpu->cpu_num,
//      cpu->fpu_state_thread ? cpu->fpu_state_thread->id : -1,
//      t->id,
//      t->fpu_cpu ? t->fpu_cpu->cpu_num : -1);

    // unset the task switched bit
    i386_clear_task_switched();

    // if our cpu owns the fpu context of the current thread, then we're done
    if (cpu->fpu_state_thread
            && cpu->fpu_state_thread == t
            && t->fpu_state_saved == false)
        return 0;

    // save the current fpu context
    if (cpu->fpu_state_thread) {
        ASSERT(cpu->fpu_state_thread->fpu_cpu == cpu);
        ASSERT(cpu->fpu_state_thread->fpu_state_saved == false);
        i386_save_fpu_context(&cpu->fpu_state_thread->arch_info.fpu_state);
        cpu->fpu_state_thread->fpu_state_saved = true;
        cpu->fpu_state_thread->fpu_cpu = NULL;
        cpu->fpu_state_thread = NULL;
    }

    // load the new context
    ASSERT(cpu->fpu_state_thread == NULL);
    ASSERT(t->fpu_cpu == NULL);
    i386_load_fpu_context(t->arch_info.fpu_state);
    t->fpu_state_saved = false;
    t->fpu_cpu = cpu;
    t->fpu_cpu->fpu_state_thread = t;

//  panic("device not available\n");

    return 0;
}

