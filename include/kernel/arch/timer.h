/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARCH_TIMER_H
#define _ARCH_TIMER_H

#include <boot/stage2.h>

void arch_timer_set_hardware_timer(time_t timeout);
void arch_timer_clear_hardware_timer();
int arch_init_timer(kernel_args *ka);

#endif

