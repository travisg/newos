/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_NET_NET_H
#define _NEWOS_KERNEL_NET_NET_H

#include <boot/stage2.h>

int net_init(kernel_args *ka);
int net_init_postdev(kernel_args *ka);

enum {
	ADDR_TYPE_NULL = 0,
	ADDR_TYPE_ETHERNET,
	ADDR_TYPE_IP
};

typedef struct netaddr {
	uint8 len;
	uint8 type;
	uint8 addr[14];
} netaddr;

#define NET_CHATTY 0

#endif

