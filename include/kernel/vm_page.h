/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_VM_PAGE_H
#define _KERNEL_VM_PAGE_H

#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/vm.h>

int vm_page_init(kernel_args *ka);
int vm_page_init_postheap(kernel_args *ka);
int vm_page_init2(kernel_args *ka);
int vm_page_init_postthread(kernel_args *ka);

int vm_mark_page_inuse(addr_t page);
int vm_mark_page_range_inuse(addr_t start_page, addr_t len);
int vm_page_set_state(vm_page *page, int state);

vm_page *vm_page_allocate_page(int state);
vm_page *vm_page_allocate_page_run(int state, addr_t len);
vm_page *vm_page_allocate_specific_page(addr_t page_num, int state);
vm_page *vm_lookup_page(addr_t page_num);

#endif

