/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_H
#define _KERNEL_H

#include <kernel/ktypes.h>
#include <kernel/arch/kernel.h>
#include <newos/defines.h>
#include <newos/errors.h>
#include <boot/stage2.h>

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
#define ROUNDOWN(a, b) (((a) / (b)) * (b))

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define CHECK_BIT(a, b) ((a) & (1 << (b)))
#define SET_BIT(a, b) ((a) | (1 << (b)))
#define CLEAR_BIT(a, b) ((a) & (~(1 << (b))))

#define _PACKED __attribute__((packed))

#define TOUCH(x) ((void)(x))

#define containerof(ptr, type, member) \
	((type *)((addr_t)(ptr) - offsetof(type, member)))

extern bool kernel_startup;
extern kernel_args global_kernel_args;

#endif

