/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _SH4_CPU_H
#define _SH4_CPU_H

#include <arch/sh4/sh4.h>

unsigned int get_fpscr();
unsigned int get_sr();
void sh4_context_switch(unsigned int **old_sp, unsigned int *new_sp);
void sh4_function_caller();
void sh4_set_kstack(addr kstack);
void sh4_enter_uspace(addr entry, addr ustack_top);
void sh4_set_user_pgdir(addr pgdir);

#endif

