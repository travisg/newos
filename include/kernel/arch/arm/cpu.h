/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARM_CPU_H
#define _ARM_CPU_H

#include <arch/cpu.h>

struct arch_cpu_info {
    // empty
};

struct iframe {
    // XXX define
};

#if 0
/* stuff defined in arch_asm.S */
void getibats(int bats[8]);
void setibats(int bats[8]);
void getdbats(int bats[8]);
void setdbats(int bats[8]);
unsigned int *getsdr1(void);
void setsdr1(unsigned int sdr);
unsigned int getsr(unsigned int va);
void setsr(unsigned int va, unsigned int val);
unsigned int getmsr(void);
void setmsr(unsigned int val);
unsigned int gethid0(void);
void sethid0(unsigned int val);
unsigned int getl2cr(void);
void setl2cr(unsigned int val);
long long get_time_base(void);

void ppc_context_switch(addr_t *old_sp, addr_t new_sp);
#endif

#endif

