/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_TIME_H
#define _NEWOS_KERNEL_ARCH_TIME_H

#include <boot/stage2.h>

int arch_time_init(kernel_args *ka);
void arch_time_tick(void);
bigtime_t arch_get_time_delta(void);
bigtime_t arch_get_rtc_delta(void);

#endif

