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

	err = sys_create("/dev", "", STREAM_TYPE_DIR);
	if(err < 0)
		panic("dev_init: error making /dev!\n");

	// bootstrap the bootfs
	bootstrap_bootfs();
	
	err = sys_create("/boot", "", STREAM_TYPE_DIR);
	if(err < 0)
		panic("error creating /boot\n");
	err = sys_mount("/boot", "bootfs");
	if(err < 0)
		panic("error mounting bootfs\n");
	
	return 0;
}
