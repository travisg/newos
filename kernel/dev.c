/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/dev.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>

#include <kernel/fs/bootfs.h>

int dev_init(kernel_args *ka)
{
	int err;

	dprintf("dev_init: entry\n");

	return 0;
}
