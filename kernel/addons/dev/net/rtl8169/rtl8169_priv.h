/*
** Copyright 2001-2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _RTL8169_PRIV_H
#define _RTL8169_PRIV_H

#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/smp.h>
#include "rtl8169_dev.h"

typedef struct rtl8169 {
    struct rtl8169 *next; // next in the list of rtl8169 devices

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
    struct rtl_tx_descriptor *txdesc;
    addr_t txdesc_phys;
    region_id txbuf_region;
    uint8 *txbuf;
    int tx_idx_free; // first free descriptor (owned by us)
    int tx_idx_full; // first full descriptor (owned by the NIC)
    sem_id tx_sem;

    region_id rxdesc_region;
    struct rtl_rx_descriptor *rxdesc;
    addr_t rxdesc_phys;
    region_id rxbuf_region;
    uint8 *rxbuf;
    int rx_idx_free; // first free descriptor (owned by us)
    int rx_idx_full; // first full descriptor (owned by the NIC)
    sem_id rx_sem;
} rtl8169;

#if 0
#define RXDESC_COUNT 256
#define TXDESC_COUNT 256
#endif

#define BUFSIZE_PER_FRAME 2048

int rtl8169_detect(rtl8169 **rtl);
int rtl8169_init(rtl8169 *rtl);
void rtl8169_xmit(rtl8169 *rtl, const char *ptr, ssize_t len);
ssize_t rtl8169_rx(rtl8169 *rtl, char *buf, ssize_t buf_len);

#endif
