/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARCH_INT_H
#define _ARCH_INT_H

int arch_int_init(kernel_args *ka);
int arch_int_init2(kernel_args *ka);

void arch_int_enable_interrupts();
int arch_int_disable_interrupts();
void arch_int_restore_interrupts(int oldstate);
void arch_int_enable_io_interrupt(int irq);
void arch_int_disable_io_interrupt(int irq);

#endif

