/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _DEV_H
#define _DEV_H

#include <kernel/kernel.h>
#include <boot/stage2.h>

int dev_init(kernel_args *ka);
image_id dev_load_dev_module(const char *name);

#endif

