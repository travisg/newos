#ifndef _SH4_CPU_H
#define _SH4_CPU_H

#include <sh4.h>

unsigned int get_fpscr();
unsigned int get_sr();
void sh4_context_switch(unsigned int **old_sp, unsigned int *new_sp);
void sh4_function_caller();

#endif

