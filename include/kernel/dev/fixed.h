/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_DEV_FIXED_H
#define _NEWOS_KERNEL_DEV_FIXED_H

#include <boot/stage2.h>

int fixed_devs_init(kernel_args *ka);

/* fixed drivers */
int null_dev_init(kernel_args *ka);
int zero_dev_init(kernel_args *ka);
int fb_console_dev_init(kernel_args *ka);
int tty_dev_init(kernel_args *ka);
int dprint_dev_init(kernel_args *ka);

#endif
