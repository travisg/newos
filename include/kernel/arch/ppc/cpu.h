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
	unsigned long srr0;
	unsigned long srr1;
	unsigned long dar;
	unsigned long dsisr;
	unsigned long lr;
	unsigned long cr;
	unsigned long xer;
	unsigned long ctr;
	unsigned long r31;
	unsigned long r30;
	unsigned long r29;
	unsigned long r28;
	unsigned long r27;
	unsigned long r26;
	unsigned long r25;
	unsigned long r24;
	unsigned long r23;
	unsigned long r22;
	unsigned long r21;
	unsigned long r20;
	unsigned long r19;
	unsigned long r18;
	unsigned long r17;
	unsigned long r16;
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long r7;
	unsigned long r6;
	unsigned long r5;
	unsigned long r4;
	unsigned long r3;
	unsigned long r2;
	unsigned long r1;
	unsigned long r0;
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
void setmsr(unsigned int val);
unsigned int gethid0(void);
void sethid0(unsigned int val);
unsigned int getl2cr(void);
void setl2cr(unsigned int val);
long long get_time_base(void);

#endif

