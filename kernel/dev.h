#ifndef _DEV_H
#define _DEV_H

#include <stage2.h>

int dev_init(kernel_args *ka);

struct device_hooks {
	int (*init_driver)(void);
};

#endif

