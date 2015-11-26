/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/time.h>
#include <kernel/arch/time.h>
#include <string.h>

// The 'rough' counter of system up time, accurate to the rate at
// which the timer interrupts run (see TICK_RATE in timer.c).
static bigtime_t sys_time;

// The delta of usecs that needs to be added to sys_time to get real time
static bigtime_t real_time_delta;

// Any additional delta that might need to be added to the above two values
// to get real time. (in case the RTC isn't in UTC)
static bigtime_t tz_delta;

// The current name of the timezone, saved for all to see
static char tz_name[SYS_MAX_NAME_LEN];

int time_init(kernel_args *ka)
{
    dprintf("time_init: entry\n");

    sys_time = 0;
    real_time_delta = 0;
    tz_delta = 0;
    strcpy(tz_name, "UTC");

    return arch_time_init(ka);
}

int time_init2(kernel_args *ka)
{
    real_time_delta = arch_get_rtc_delta();

    return 0;
}

void time_tick(int tick_rate)
{
    sys_time += tick_rate;
    arch_time_tick();
}

bigtime_t system_time(void)
{
    volatile bigtime_t *st = &sys_time;
    bigtime_t val;

retry:
    // read the system time, make sure we didn't get a partial read
    val = sys_time;
    if (val > *st)
        goto retry;

    // ask the architectural specific layer to give us a little better precision if it can
    val += arch_get_time_delta();

    return val;
}

bigtime_t system_time_lores(void)
{
    volatile bigtime_t *st = &sys_time;
    bigtime_t val;

retry:
    // read the system time, make sure we didn't get a partial read
    val = sys_time;
    if (val > *st)
        goto retry;

    return val;
}

bigtime_t local_time(void)
{
    return system_time() + real_time_delta + tz_delta;
}

