/* $Id$
**
** Copyright 2001 Brian J. Swetland
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions, and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions, and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
**  PCI GAPS & Original 8139C Code:
**
**  Naive RealTek 8139C driver for the DC Broadband Adapter. Some 
**  assistance on names of things from the NetBSD-DC sources.
**  
**  (c)2001 Dan Potter
**  License: X11
**
*/

#include "defs.h"
#include <kernel/debug.h>
#include <nulibc/string.h>
#include <nulibc/stdarg.h>

#include "rtl8139c.h"

/* Convienence macros */
#define REGL(a) (volatile unsigned long *)(a)
#define vul volatile unsigned long
#define REGS(a) (volatile unsigned short *)(a)
#define vus volatile unsigned short
#define REGC(a) (volatile unsigned char *)(a)
#define vuc volatile unsigned char

/* Configuration definitions */
#define RX_BUFFER_LEN		8192

#define ADDR_RXBUF 0x01840000
#define ADDR_TXBUF0 0x01846000

/* Mac address of card */
u1 rtl_mac[6];

/* 8, 16, and 32 bit access to the PCI I/O space (configured by GAPS) */
static vuc * const nic8 = REGC(0xa1001700);
static vus * const nic16 = REGS(0xa1001700);
static vul * const nic32 = REGL(0xa1001700);

/* 8 and 32 bit access to the PCI MEMMAP space (configured by GAPS) */
static vuc * const mem8 = REGC(0xa1840000);
static vul * const mem32 = REGL(0xa1840000);

/*
  Detect the presence of a Broadband Adapter

  This really detects a "GAPS PCI" bridge chip, but the only device
  for the Dreamcast that uses it is the net card.
  
  Returns 0 if one is present, -1 if one is not.
 */
int rtl8139_detect() {
	const char *str = (char*)REGC(0xa1001400);
	const char *chk = "GAPSPCI_BRIDGE_2";
	int i;
	
	for(i = 0; i < 16; i++){
		if(str[i] != chk[i]) return -1;
	}
	return 0;
}

int bb_exit()
{
	int i;
	dprintf("rtl8139c: shutdown\n");

	/* Soft-reset the chip */
	nic8[RT_CHIPCMD] = RT_CMD_RESET;
	
	/* Wait for it to come back */
	i = 1000000;
	while ((nic8[RT_CHIPCMD] & RT_CMD_RESET) && i > 0)
		i--;

	if(i == 0) {
		dprintf("rtl8139: CANNOT RESET\n");
	} else {
		dprintf("rtl8139: RESET OKAY\n");
	}
	return 0;
}
/*
  Initializes the BBA
  
  Returns 0 for success or -1 for failure.
 */
int rtl8139_init() {
	vuc * const g28 = REGC(0xa1000000);
	vus * const g216 = REGS(0xa1000000);
	vul * const g232 = REGL(0xa1000000);

	int i;
	u4 tmp;
	
	
	/* Initialize the "GAPS" PCI glue controller.
	   It ain't pretty but it works. */
	g232[0x1418/4] = 0x5a14a501;		/* M */
	i = 10000;
	while (!(g232[0x1418/4] & 1) && i > 0)
		i--;
	if (!(g232[0x1418/4] & 1)) {
		dprintf("rtl8139: GAPS PCI controller not responding; giving up!\n");
		return -1;
	}
	g232[0x1420/4] = 0x01000000;
	g232[0x1424/4] = 0x01000000;
	g232[0x1428/4] = 0x01840000;		/* DMA Base */
	g232[0x142c/4] = 0x01840000 + 32*1024;	/* DMA End */
	g232[0x1414/4] = 0x00000001;
	g232[0x1434/4] = 0x00000001;

	/* Configure PCI bridge (very very hacky). If we wanted to be proper,
	   we ought to implement a full PCI subsystem. In this case that is
	   ridiculous for accessing a single card that will probably never
	   change. Considering that the DC is now out of production officially,
	   there is a VERY good chance it will never change. */
	g216[0x1606/2] = 0xf900;
	g232[0x1630/4] = 0x00000000;
	g28[0x163c] = 0x00;
	g28[0x160d] = 0xf0;
	(void)g216[0x04/2];
	g216[0x1604/2] = 0x0006;
	g232[0x1614/4] = 0x01000000;
	(void)g28[0x1650];
	
	memset((void *)PHYS_ADDR_TO_P1(ADDR_RXBUF), 0, 32*1024);

	/* Soft-reset the chip */
	nic8[RT_CHIPCMD] = RT_CMD_RESET;
	
	/* Wait for it to come back */
	i = 1000000;
	while ((nic8[RT_CHIPCMD] & RT_CMD_RESET) && i > 0)
		i--;

	if(i == 0) {
		dprintf("rtl8139: CANNOT RESET\n");
		return -1;
	} else {
		dprintf("rtl8139: RESET OKAY\n");
	}
	dprintf("0x%x\n", nic16[RT_RXBUFHEAD/2]);
	/* Reset CONFIG1 */
	nic8[RT_CONFIG1] = 0;
	
	/* Enable auto-negotiation and restart that process */
	nic16[RT_MII_BMCR/2] = 0x1200;
	for(i = 0; i < 100000; i++);
	
	/* Do another reset */
#if 0
	nic8[RT_CHIPCMD] = RT_CMD_RESET;
	
	/* Wait for it to come back */
	i = 1000;
	while ((nic8[RT_CHIPCMD] & RT_CMD_RESET) && i > 0)
		i--;
#endif
	
	/* Enable writing to the config registers */
//	nic8[RT_CFG9346] = 0xc0;
	
	/* Read MAC address */
	tmp = nic32[RT_IDR0];
	rtl_mac[0] = tmp & 0xff;
	rtl_mac[1] = (tmp >> 8) & 0xff;
	rtl_mac[2] = (tmp >> 16) & 0xff;
	rtl_mac[3] = (tmp >> 24) & 0xff;
	tmp = nic32[RT_IDR0+1];
	rtl_mac[4] = tmp & 0xff;
	rtl_mac[5] = (tmp >> 8) & 0xff;
	dprintf("rtl8139: MAC ADDR %X:%X:%X:%X:%X:%X\n",
		rtl_mac[0], rtl_mac[1], rtl_mac[2],
		rtl_mac[3], rtl_mac[4], rtl_mac[5]);

	/* Enable receive and transmit functions */
	nic8[RT_CHIPCMD] = RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE;

	/* Set Rx FIFO threashold to 1K, Rx size to 8k+16, 1024 byte DMA burst, WRAP mode */
	nic32[RT_RXCONFIG/4] = 0x0000c680;
	
	/* Set Tx 1024 byte DMA burst */
	nic32[RT_TXCONFIG/4] = 0x03000600;

	/* Turn off lan-wake and set the driver-loaded bit */
	nic8[RT_CONFIG1] = (nic8[RT_CONFIG1] & ~0x30) | 0x20;
	
	/* Enable FIFO auto-clear */
	nic8[RT_CONFIG4] |= 0x80;
	
	/* Switch back to normal operation mode */
	nic8[RT_CFG9346] = 0;
	
	/* Setup Rx buffers */
	nic32[RT_RXBUF/4] = ADDR_RXBUF;
	nic32[RT_TXADDR0/4 + 0] = ADDR_TXBUF0;
	nic32[RT_TXADDR0/4 + 1] = 0x01846800;
	nic32[RT_TXADDR0/4 + 2] = 0x01847000;
	nic32[RT_TXADDR0/4 + 3] = 0x01847800;
	
	/* Reset RXMISSED counter */
	nic32[RT_RXMISSED/4] = 0;
	
	/* Enable receiving broadcast and physical match packets */
//	nic32[RT_RXCONFIG/4] |= 0x0000000a;
	nic32[RT_RXCONFIG/4] |= 0x0000000f;
	
	/* Filter out all multicast packets */
	nic32[RT_MAR0/4 + 0] = 0;
	nic32[RT_MAR0/4 + 1] = 0;
	
	/* Disable all multi-interrupts */
	nic16[RT_MULTIINTR/2] = 0;

//	nic16[RT_INTRMASK/2] = 0x807f;
	nic16[RT_INTRMASK/2] = 0;
	
	/* Enable RX/TX once more */
	nic8[RT_CHIPCMD] = RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE;

	nic8[RT_CFG9346] = 0;

	dprintf("rtl8139: ready.\n");
	
	return 0;
}

typedef struct 
{
	volatile u2 status;
	volatile u2 len;
	volatile u1 data[1];
} rx_entry;

#define RXSIZE (8192)

u1 *rxend = (u1*) (PHYS_ADDR_TO_P1(ADDR_RXBUF + RXSIZE));
u1 *rxptr = (u1*) (PHYS_ADDR_TO_P1(ADDR_RXBUF));
u1 *rxstart = (u1*) (PHYS_ADDR_TO_P1(ADDR_RXBUF));

static int txbn = 0;

void rtl8139_xmit(const char *ptr, int len)
{
	int i;
		dprintf("XMIT(%d) %x (%d)\n",txbn,ptr,len);
		/* wait for clear-to-send */
	while(!(nic32[txbn + RT_TXSTATUS0/4] & 0x2000)) {
		if(nic32[txbn + RT_TXSTATUS0/4] & 0x40000000){
			dprintf("rtl8139: XMIT STATUS ABORT!?\n");
			nic32[txbn + RT_TXCONFIG/4] = 0x03000601;
		}
		for(i = 0; i < 1000; i++);
	}
	
	memcpy((void*) ((txbn * 0x800) + PHYS_ADDR_TO_P1(ADDR_TXBUF0)), ptr, len);
	if(len < 64) len = 64;
	
	nic32[txbn + RT_TXADDR0/4] = (txbn * 0x800) + ADDR_TXBUF0;
	nic32[txbn + RT_TXSTATUS0/4] = len | 0x80000;

#if 0
		/* wait for clear-to-send */
	while(!(nic32[txbn + RT_TXSTATUS0/4] & (0x20000 | 0x4000))) {
		for(i = 0; i < 100; i++);
	}
#endif
		dprintf("XMIT DONE %x\n",nic32[txbn + RT_TXSTATUS0/4]);

	txbn = (txbn + 1) & 3;
	
}

int rtl8139_rx(char *buf, int buf_len)
{
	int len, status, i, crx;
	int processed = 0;
	rx_entry *rxe;

//	if(1) {
	dprintf("0x%x\n", nic8[RT_CHIPCMD]);
	if (!(nic8[RT_CHIPCMD] & 1)) {
		rxe = (rx_entry*) rxptr;
		len = rxe->len;
		status = rxe->status;

		crx = nic16[RT_RXBUFTAIL/2];

#if 1
		dprintf("RX: @%x L:%x S:%x CRX:%x\n",rxptr,len,status,crx);
#endif
		
		if(len > 1500) {
			panic("rtl8139_rx: frame too large 0x%x\n", len);
		}
		if(status & 1){
			memcpy(buf, (char *)rxe->data, len - 4);
			processed++;
		} else {
				/* error */
		}

			/* advance to next packet */
		rxptr += ((len + 4 + 3) & ~3);
		if(rxptr >= rxend){
			rxptr = rxstart + (rxptr - rxend);
		}

			/* tell the 8139 where we are */
		nic16[RT_RXBUFTAIL/2] = ((u4) (rxptr - rxstart)) - 16;
	}

	return processed;
}

int chatter = 1;

int rtl8139_foo()
{
	for(;;) {
		dprintf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", 
			(int)nic8[RT_CHIPCMD], (int)nic32[RT_RXCONFIG/4], 
			(int)nic8[RT_MEDIASTATUS],
			(int)nic16[RT_INTRSTATUS/2],
			(int)nic32[RT_RXBUF/4], 
			(int)nic16[RT_RXBUFHEAD/2],
			(int)nic32[RT_RXMISSED/4]
			);
//		nic16[RT_RXBUFHEAD] = 0;
//		if(nic16[RT_INTRSTATUS/2] == 0x20) 
//			panic("y0\n");
	}
	return 0;
}
