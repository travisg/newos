/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _MAPLE_BUS_H
#define _MAPLE_BUS_H

#include <boot/stage2.h>

enum {
	MAPLE_IOCTL_GET_FUNC = 9876,
};

int maple_bus_init(kernel_args *ka);

#endif
