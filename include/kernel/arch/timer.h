/* 
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_TIMER_H
#define _NEWOS_KERNEL_ARCH_TIMER_H

#include <boot/stage2.h>

enum {
	HW_TIMER_ONESHOT,
	HW_TIMER_REPEATING
};

void arch_timer_set_hardware_timer(bigtime_t timeout, int type);
void arch_timer_clear_hardware_timer(void);
int arch_init_timer(kernel_args *ka);

#endif

