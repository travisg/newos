/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARCH_VM_H
#define _ARCH_VM_H

#include <boot/stage2.h>

int arch_vm_init(kernel_args *ka);
int arch_vm_init2(kernel_args *ka);

#endif

