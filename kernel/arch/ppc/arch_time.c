/*
** Copyright 2003-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <kernel/kernel.h>
#include <kernel/debug.h>

#include <kernel/time.h>
#include <kernel/arch/time.h>

void ppc_timer_reset(void);

int arch_time_init(kernel_args *ka)
{
    return 0;
}

void arch_time_tick(void)
{
    // kick the decrementer over
    ppc_timer_reset();
}

bigtime_t arch_get_time_delta(void)
{
    // XXX implement
    return 0;
}

bigtime_t arch_get_rtc_delta(void)
{
    // XXX implement. Return RTC time in usecs since 0AD
    return 0;
}

