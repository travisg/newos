/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _PPC_CPU_H
#define _PPC_CPU_H

#include <arch/cpu.h>

struct arch_cpu_info {
	// empty
};


struct iframe {
	// empty
};

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
void setmsr(unsigned int msr);
long long get_time_base(void);

#endif

