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


#endif

