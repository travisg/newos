/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_HEAP_H
#define _KERNEL_HEAP_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

int heap_init(addr_t new_heap_base, unsigned int new_heap_size);
int heap_init_postsem(kernel_args *ka);
void *kmalloc(unsigned int size);
void kfree(void *address);
char *kstrdup(const char*text);
#endif

