#ifndef _ARCH_CPU_H
#define _ARCH_CPU_H

#include <kernel.h>
#include <stage2.h>
#include <sh4.h>

#define PAGE_ALIGN(x) (((x) + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1))

int atomic_add(int *val, int incr);
int atomic_and(int *val, int incr);
int atomic_or(int *val, int incr);
int test_and_set(int *val, int set_to);

time_t system_time();
int arch_cpu_init(kernel_args *ka);
void reboot();

unsigned int get_sr();
void sh4_context_switch(unsigned int **old_sp, unsigned int *new_sp);
void sh4_function_caller();

#endif

