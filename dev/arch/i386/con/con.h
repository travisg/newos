#ifndef _CONSOLE_DEV_H
#define _CONSOLE_DEV_H

int console_dev_init(kernel_args *ka);

enum {
	CONSOLE_OP_WRITEXY = 2376
};

#endif
