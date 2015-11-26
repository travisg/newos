/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NS82830_DEV_H
#define _NS82830_DEV_H

/* vendor id for the National Semiconductor 83820/83821 cards */
#define NS_VENDOR_ID 0x100b

/* device id */
#define NS_DEVICE_ID_83820 0x0022

/* registers */
#define REG_CR    0x0
#define REG_CFG   0x4
#define REG_MEAR  0x8
#define REG_PTSCR 0xc
#define REG_ISR   0x10
#define REG_IMR   0x14
#define REG_IER   0x18
#define REG_IHR   0x1c
#define REG_TXDP  0x20
#define REG_TXCFG 0x28
#define REG_GPIOR 0x2c
#define REG_RXDP  0x30
#define REG_RXCFG 0x38
#define REG_PQCR  0x3c
#define REG_WCSR  0x40
#define REG_PCR   0x44
#define REG_RFCR  0x48
#define REG_RFDR  0x4c
#define REG_BRAR  0x50
#define REG_BRDR  0x54
#define REG_SRR   0x58
#define REG_MIBC  0x5c
#define REG_MIB   0x60
#define REG_TXDP1 0xa0
#define REG_TXDP2 0xa4
#define REG_TXDP3 0xa8
#define REG_RXDP1 0xb0
#define REG_RXDP2 0xb4
#define REG_RXDP3 0xb8
#define REG_VRCR  0xbc
#define REG_VTCR  0xc0
#define REG_VDR   0xc4
#define REG_CCSR  0xcc
#define REG_TBICR 0xe0
#define REG_TBISR 0xe4
#define REG_TANAR 0xe8
#define REG_TANLPAR 0xec
#define REG_TANER 0xf0
#define REG_TESR  0xf4
#define REG_PCI_CFG_BASE 0x200

struct ns_desc_32 {
    uint32 link;
    uint32 bufptr;
    uint32 cmdsts;
    uint32 extsts;

    // our data
    struct ns_desc_32 *next;
};

struct ns_desc_64 {
    uint64 link;
    uint64 bufptr;
    uint32 cmdsts;
    uint32 extsts;

    // our data
    struct ns_desc_32 *next;
};

#endif
