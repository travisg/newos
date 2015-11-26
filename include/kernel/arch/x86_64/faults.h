/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_X86_64_FAULTS
#define _NEWOS_KERNEL_ARCH_X86_64_FAULTS

int x86_64_general_protection_fault(int errorcode);
int x86_64_double_fault(int errorcode);

#endif

