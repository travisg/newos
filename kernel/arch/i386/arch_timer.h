#ifndef _ARCH_TIMER_H
#define _ARCH_TIMER_H

int arch_timer_set_hardware_timer(long long timeout);
int arch_init_timer(struct kernel_args *ka);

#endif

