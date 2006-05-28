/*
** Copyright 2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _RHINE_DEV_H
#define _RHINE_DEV_H

#define RHINE_PAR0 0x0
#define RHINE_PAR1 0x1
#define RHINE_PAR2 0x2
#define RHINE_PAR3 0x3
#define RHINE_PAR4 0x4
#define RHINE_PAR5 0x5
#define RHINE_RCR  0x6
#define RHINE_TCR  0x7
#define RHINE_CR0  0x8
#define RHINE_CR1  0x9
#define RHINE_ISR0 0xc
#define RHINE_ISR1 0xd
#define RHINE_IMR0 0xe
#define RHINE_IMR1 0xf
#define RHINE_MAR0 0x10
#define RHINE_MAR1 0x11
#define RHINE_MAR2 0x12
#define RHINE_MAR3 0x13
#define RHINE_MAR4 0x14
#define RHINE_MAR5 0x15
#define RHINE_MAR6 0x16
#define RHINE_MAR7 0x17
#define RHINE_RDA0 0x18
#define RHINE_RDA1 0x19
#define RHINE_RDA2 0x1a
#define RHINE_RDA3 0x1b
#define RHINE_TDA0 0x1c
#define RHINE_TDA1 0x1d
#define RHINE_TDA2 0x1e
#define RHINE_TDA3 0x1f
#define RHINE_MPHY 0x6c
#define RHINE_MIISR 0x6d
#define RHINE_BCR0 0x6e
#define RHINE_BCR1 0x6f
#define RHINE_MIICR 0x70
#define RHINE_MIIAD 0x71
#define RHINE_EECSR 0x74
#define RHINE_TEST 0x75
#define RHINE_CFGA 0x78
#define RHINE_CFGB 0x79
#define RHINE_CFGC 0x7a
#define RHINE_CFGD 0x7b
#define RHINE_MPAC0 0x7c
#define RHINE_MPAC1 0x7d
#define RHINE_CRCC0 0x7e
#define RHINE_CRCC1 0x7f
#define RHINE_MISC_CR1 0x81

struct rhine_rx_desc {
	uint16 status;
	uint16 framelen; // also owner bit
	uint32 buflen;
	uint32 ptr;
	uint32 next;
};
#define RHINE_RX_OWNER (1<<15)
#define RHINE_RX_CHAIN (1<<15)

// XXX fix
struct rhine_tx_desc {
	uint16 status;
	uint16 framelen; // also owner bit
	uint32 buflen;
	uint32 ptr;
	uint32 next;
};

#endif
