/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_I386_INTERRUPTS_H
#define _NEWOS_KERNEL_ARCH_I386_INTERRUPTS_H

void trap0(void);
void trap1(void);
void trap2(void);
void trap3(void);
void trap4(void);
void trap5(void);
void trap6(void);
void trap7(void);
void trap8(void);
void trap9(void);
void trap10(void);
void trap11(void);
void trap12(void);
void trap13(void);
void trap14(void);
void trap16(void);
void trap17(void);
void trap18(void);
void trap32(void);
void trap33(void);
void trap34(void);
void trap35(void);
void trap36(void);
void trap37(void);
void trap38(void);
void trap39(void);
void trap40(void);
void trap41(void);
void trap42(void);
void trap43(void);
void trap44(void);
void trap45(void);
void trap46(void);
void trap47(void);

void trap251(void);
void trap252(void);
void trap253(void);
void trap254(void);
void trap255(void);

void i386_syscall_vector(void);

#endif

