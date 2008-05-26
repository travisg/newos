/*
** Copyright 2001-2008, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <stdio.h>
#include "stage2_priv.h"

static unsigned char *heap_ptr = (unsigned char *)0x01000000; // 16MB

void *kmalloc(unsigned int size)
{
//	dprintf("kmalloc: size %d, ptr %p\n", size, heap_ptr - size);

	return (heap_ptr -= size);
}

void kfree(void *ptr)
{
}

