/* 
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_TIME_H
#define _KERNEL_TIME_H

#include <boot/stage2.h>

int time_init(kernel_args *ka);
void time_tick(int tick_rate);

// microseconds since the system was booted
bigtime_t system_time(void);

// microseconds since the system was booted, accurate only to a few milliseconds
bigtime_t system_time_lores(void);

#endif

