#ifndef _ARCH_TIMER_H
#define _ARCH_TIMER_H

#include <stage2.h>

void arch_timer_set_hardware_timer(time_t timeout);
int arch_init_timer(kernel_args *ka);

#endif

