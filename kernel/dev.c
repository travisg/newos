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
#include <string.h>

int dev_init(kernel_args *ka)
{
	dprintf("dev_init: entry\n");

	return 0;
}

int dev_scan_drivers(kernel_args *ka)
{
	int fd;

	fd = sys_opendir("/boot/addons/dev");
	if(fd >= 0) {
		ssize_t len;
		char buf[SYS_MAX_NAME_LEN];

		while((len = sys_readdir(fd, buf, sizeof(buf))) > 0) {
			dprintf("loading '%s' dev module\n", buf);
			dev_load_driver(buf);
		}
		sys_closedir(fd);
	}
	return NO_ERROR;
}


image_id dev_load_driver(const char *name)
{
	image_id id;
	int (*bootstrap)();
	int err;
	char path[SYS_MAX_PATH_LEN];

	strcpy(path, "/boot/addons/dev/");
	strlcat(path, name, sizeof(path));

	id = elf_load_kspace(path, "");
	if(id < 0)
		return id;

	bootstrap = (void *)elf_lookup_symbol(id, "dev_bootstrap");
	if(!bootstrap) {
		dprintf("dev_load_driver: driver '%s' does not have dev_bootstrap() call\n", name);
		elf_unload_kspace(name);
		return ERR_INVALID_BINARY;
	}

	// call the driver's bootstrap routine
	err = bootstrap();
	if(err < NO_ERROR) {
		dprintf("dev_load_driver: driver '%s' bootstrap returned error, unloading...\n", name);
		elf_unload_kspace(name);
		return ERR_NOT_FOUND;
	}

	return id;
}

