/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _VM_STORE_ANONYMOUS_H
#define _VM_STORE_ANONYMOUS_H

#include <kernel/kernel.h>
#include <kernel/vm.h>

vm_store *vm_store_create_anonymous_noswap();

#endif

