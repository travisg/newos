#ifndef _DEV_H
#define _DEV_H

#include <kernel.h>
#include <stage2.h>

int dev_init(kernel_args *ka);

struct device_hooks {
	int (*driver_open)(const char *name, int flags, void **cookie);
	int (*driver_close)(void *cookie);
	int (*driver_read)(void *cookie, off_t pos, void *data, size_t *num_bytes);
	int (*driver_write)(void *cookie, off_t pos, const void *data, size_t *num_bytes);
};

#endif

