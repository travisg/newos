/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_ARCH_PPC_CPU_H
#define _NEWOS_ARCH_PPC_CPU_H

#define PAGE_SIZE 4096

#define _BIG_ENDIAN 1
#define _LITTLE_ENDIAN 0

// BAT register defs
#define BATU_BEPI_MASK	0xfffe0000
#define BATU_LEN_128K	0x0
#define BATU_LEN_256K	0x4
#define BATU_LEN_512K	0xc
#define BATU_LEN_1M		0x1c
#define BATU_LEN_2M		0x3c
#define BATU_LEN_4M		0x7c
#define BATU_LEN_8M		0xfc
#define BATU_LEN_16M	0x1fc
#define BATU_LEN_32M	0x3fc
#define BATU_LEN_64M	0x7fc
#define BATU_LEN_128M	0xffc
#define BATU_LEN_256M	0x1ffc
#define BATU_VS			0x2
#define BATU_VP			0x1

#define BATL_BRPN_MASK	0xfffe0000
#define BATL_WIMG_MASK	0x78
#define BATL_WT			0x40
#define BATL_CI			0x20
#define BATL_MC			0x10
#define BATL_G			0x08
#define BATL_PP_MASK	0x3
#define BATL_PP_RO		0x1
#define BATL_PP_RW		0x2

struct ppc_pte {
	// pte upper word
	unsigned int v : 1;
	unsigned int vsid : 24;
	unsigned int hash : 1;
	unsigned int api : 6;
	// pte lower word
	unsigned int ppn : 20;
	unsigned int unused : 3;
	unsigned int r : 1;
	unsigned int c : 1;
	unsigned int wimg : 4;
	unsigned int unused1 : 1;
	unsigned int pp : 2;
};

struct ppc_pteg {
	struct ppc_pte pte[8];
};

#define MSR_LE (1 << 0)
#define MSR_RI (1 << 1)
#define MSR_DR (1 << 4)
#define MSR_IR (1 << 5)
#define MSR_IP (1 << 6)
#define MSR_FE1 (1 << 8)
#define MSR_BE (1 << 9)
#define MSR_SE (1 << 10)
#define MSR_FE0 (1 << 11)
#define MSR_ME (1 << 12)
#define MSR_FP (1 << 13)
#define MSR_PR (1 << 14)
#define MSR_EE (1 << 15)
#define MSR_ILE (1 << 16)
#define MSR_POW (1 << 18)

/* mpc750 */
#define HID0_NOOPT1	(1 << 0)
#define HID0_BHT	(1 << 2)
#define HID0_ABE	(1 << 3)
#define HID0_BTIC	(1 << 5)
#define HID0_DCFA	(1 << 6)
#define HID0_SGE	(1 << 7)
#define HID0_IFEM	(1 << 8)
#define HID0_SPD	(1 << 9)
#define HID0_DCFI	(1 << 10)
#define HID0_ICFI	(1 << 11)
#define HID0_DLOCK	(1 << 12)
#define HID0_ILOCK	(1 << 13)
#define HID0_DCE	(1 << 14)
#define HID0_ICE	(1 << 15)
#define HID0_NHR	(1 << 16)
#define HID0_DPM	(1 << 20)
#define HID0_SLEEP	(1 << 21)
#define HID0_NAP	(1 << 22)
#define HID0_DOZE	(1 << 23)
#define HID0_PAR	(1 << 24)
#define HID0_ECLK	(1 << 25)
#define HID0_BCLK	(1 << 27)
#define HID0_EBD	(1 << 28)
#define HID0_EBA	(1 << 29)
#define HID0_DBP	(1 << 30)
#define HID0_EMCP	(1 << 31)

#endif

