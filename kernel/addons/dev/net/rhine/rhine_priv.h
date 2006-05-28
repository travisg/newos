/*
** Copyright 2001-2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _RHINE_PRIV_H
#define _RHINE_PRIV_H

#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/smp.h>
#include "rhine_dev.h"

typedef struct rhine {
	struct rhine *next; // next in the list of rhine devices

	int irq;
	addr_t phys_base;
	addr_t phys_size;
	addr_t virt_base;
	uint16 io_port;
	region_id region;
	uint8 mac_addr[6];

	mutex lock;
	spinlock_t reg_spinlock;

	region_id txdesc_region;
	struct rhine_tx_desc *txdesc;
	addr_t txdesc_phys;

	region_id rxdesc_region;
	struct rhine_rx_desc *rxdesc;
	addr_t rxdesc_phys;
	int rx_head; // the nic consumes rx descriptors from the head of the list
	int rx_tail; // more descriptors are filled in at the tail
} rhine;

#define RXDESC_COUNT 256
#define TXDESC_COUNT 256

int rhine_detect(rhine **rtl);
int rhine_init(rhine *rtl);
void rhine_xmit(rhine *rtl, const char *ptr, ssize_t len);
ssize_t rhine_rx(rhine *rtl, char *buf, ssize_t buf_len);

#endif
