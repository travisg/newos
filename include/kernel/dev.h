/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _DEV_H
#define _DEV_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

int dev_init(kernel_args *ka);

struct device_hooks {
	int (*driver_open)(const char *name, int flags, void **cookie);
	int (*driver_close)(void *cookie);
	int (*driver_freecookie)(void *cookie);
	int (*driver_read)(void *cookie, void *data, off_t pos, size_t *num_bytes);
	int (*driver_write)(void *cookie, const void *data, off_t pos, size_t *num_bytes);
};

#endif

