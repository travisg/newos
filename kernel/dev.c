/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/dev.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>
#include <kernel/elf.h>

#include <stdio.h>

int dev_init(kernel_args *ka)
{
	dprintf("dev_init: entry\n");

	return 0;
}

int dev_scan_drivers(kernel_args *ka)
{
	int fd;

	fd = sys_open("/boot/addons/dev", STREAM_TYPE_DIR, 0);
	if(fd >= 0) {
		ssize_t len;
		char buf[SYS_MAX_NAME_LEN];

		while((len = sys_read(fd, buf, 0, sizeof(buf))) > 0) {
			dprintf("loading '%s' dev module\n", buf);
			dev_load_dev_module(buf);
		}
		sys_close(fd);
	}
	return NO_ERROR;
}


image_id dev_load_dev_module(const char *name)
{
	image_id id;
	void (*bootstrap)();
	char path[SYS_MAX_PATH_LEN];

	sprintf(path, "/boot/addons/dev/%s", name);

	id = elf_load_kspace(path, "");
	if(id < 0)
		return id;

	bootstrap = (void *)elf_lookup_symbol(id, "dev_bootstrap");
	if(!bootstrap)
		return ERR_VFS_INVALID_FS;

	bootstrap();

	return id;
}

