/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _INT_H
#define _INT_H

#include <boot/stage2.h>
#include <kernel/arch/int.h>

int int_init(kernel_args *ka);
int int_init2(kernel_args *ka);
int int_io_interrupt_handler(int vector);
int int_set_io_interrupt_handler(int vector, int (*func)(void));

#define int_enable_interrupts	arch_int_enable_interrupts
#define int_disable_interrupts	arch_int_disable_interrupts
#define int_restore_interrupts	arch_int_restore_interrupts

enum {
	INT_NO_RESCHEDULE,
	INT_RESCHEDULE
};

#endif

