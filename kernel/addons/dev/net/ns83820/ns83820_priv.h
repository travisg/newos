/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NS83820_PRIV_H
#define _NS83820_PRIV_H

#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/smp.h>

typedef struct ns83820 {
	int irq;
	addr_t phys_base;
	addr_t phys_size;
	addr_t virt_base;
	uint16 io_port;
	region_id region;
	uint8 mac_addr[6];

	mutex lock;
	spinlock_t reg_spinlock;
} ns83820;

int ns83820_detect(ns83820 **ns, int *num);
int ns83820_init(ns83820 *ns);
void ns83820_xmit(ns83820 *ns, const char *ptr, ssize_t len);
ssize_t ns83820_rx(ns83820 *ns, char *buf, ssize_t buf_len);

#endif
