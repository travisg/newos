/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/vm.h>
#include <kernel/int.h>
#include <kernel/lock.h>
#include <kernel/sem.h>
#include <kernel/arch/cpu.h>
#include <kernel/arch/sh4/cpu.h>

#include <kernel/bus/bus.h>
#include <nulibc/string.h>

#include "rtl8139_dev.h"
#include "rtl8139_priv.h"

#define ASIC_IRQB_B (*(volatile unsigned int*)0xa05f6924)



#define RTL_WRITE_8(rtl, reg, dat) \
	*(uint8 *)((rtl)->virt_base + (reg)) = (dat)
#define RTL_WRITE_16(rtl, reg, dat) \
	*(uint16 *)((rtl)->virt_base + (reg)) = (dat)
#define RTL_WRITE_32(rtl, reg, dat) \
	*(uint32 *)((rtl)->virt_base + (reg)) = (dat)

#define RTL_READ_8(rtl, reg) \
	*(uint8 *)((rtl)->virt_base + reg)
#define RTL_READ_16(rtl, reg) \
	*(uint16 *)((rtl)->virt_base + reg)
#define RTL_READ_32(rtl, reg) \
	*(uint32 *)((rtl)->virt_base + reg)

#define MYRT_INTS (RT_INT_PCIERR | RT_INT_RX_ERR | RT_INT_RX_OK | RT_INT_TX_ERR | RT_INT_TX_OK | RT_INT_RXBUF_OVERFLOW)

#define RXBUFMASK 0x3fff
#define RXBUFSIZE 0x4000

#define ADDR_RXBUF 0x01840000
#define ADDR_TXBUF0 0x01846000


#define TAILREG_TO_TAIL(in) \
	(uint16)(((uint32)(in) + 16) % RXBUFSIZE)
#define TAIL_TO_TAILREG(in) \
	(uint16)((uint16)(in) - 16)


static int rtl8139_int(void*);

#define REGL(a) (volatile unsigned long *)(a)
#define vul volatile unsigned long
#define REGS(a) (volatile unsigned short *)(a)
#define vus volatile unsigned short
#define REGC(a) (volatile unsigned char *)(a)
#define vuc volatile unsigned char

static int gaps_init()
{
	vuc * const g28 = REGC(0xa1000000);
	vus * const g216 = REGS(0xa1000000);
	vul * const g232 = REGL(0xa1000000);
	int i;
	
	/* Initialize the "GAPS" PCI glue controller.
	   It ain't pretty but it works. */
	g232[0x1418/4] = 0x5a14a501;		/* M */
	i = 10000;
	while (!(g232[0x1418/4] & 1) && i > 0)
		i--;
	if (!(g232[0x1418/4] & 1)) {
		dprintf("gaps_init() - no pci glue available\n");
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

	dprintf("gaps_init() - pci glue controller detected\n");
	return 0 ;
}

int rtl8139_detect(rtl8139 **rtl)
{
	if(gaps_init() != 0) return -1;

	// we found one
	dprintf("rtl8139_detect: found device at '%s'\n", "gaps/0");

	*rtl = kmalloc(sizeof(rtl8139));
	if(*rtl == NULL) {
		return -1;
	}
	memset(*rtl, 0, sizeof(rtl8139));
	(*rtl)->irq = 27;
	(*rtl)->phys_base = 0x01001700;
	(*rtl)->phys_size = 0x100;	
	
	dprintf("detected rtl8139 at irq %d, memory base 0x%lx, size 0x%lx\n",
			(*rtl)->irq, (*rtl)->phys_base, (*rtl)->phys_size);

	return 0;
}

int rtl8139_init(rtl8139 *rtl)
{
	bigtime_t time;
	int err = -1;
	addr temp;

	dprintf("rtl8139_init: rtl %p\n", rtl);

	rtl->region = vm_map_physical_memory(vm_get_kernel_aspace_id(), "rtl8139_region", (void **)&rtl->virt_base,
		REGION_ADDR_ANY_ADDRESS, rtl->phys_size, LOCK_KERNEL|LOCK_RW, rtl->phys_base);
	if(rtl->region < 0) {
		dprintf("rtl8139_init: error creating memory mapped region\n");
		err = -1;
		goto err;
	}
	dprintf("rtl8139 mapped at address 0x%lx\n", rtl->virt_base);

	// try to reset the device
 	time = system_time();
	RTL_WRITE_8(rtl, RT_CHIPCMD, RT_CMD_RESET);
	do {
		thread_snooze(10000); // 10ms
		if(system_time() - time > 1000000) {
			err = -1;
			dprintf("rtl8139 reset timed out\n");
			goto err1;
		}
	} while((RTL_READ_8(rtl, RT_CHIPCMD) & RT_CMD_RESET));

	// create a rx and tx buf
	rtl->rxbuf = PHYS_ADDR_TO_P1(ADDR_RXBUF);
	rtl->txbuf = PHYS_ADDR_TO_P1(ADDR_TXBUF0);

		
	// set up the transmission buf and sem
	rtl->tx_sem = sem_create(4, "rtl8139_txsem");
	mutex_init(&rtl->lock, "rtl8139");
	rtl->txbn = 0;
	rtl->last_txbn = 0;
	rtl->rx_sem = sem_create(0, "rtl8139_rxsem");
	rtl->reg_spinlock = 0;

	// set up the interrupt handler
	int_set_io_interrupt_handler(rtl->irq, &rtl8139_int, rtl);
	
	ASIC_IRQB_B = 1 << 3;
	
	// read the mac address
	rtl->mac_addr[0] = RTL_READ_8(rtl, RT_IDR0);
	rtl->mac_addr[1] = RTL_READ_8(rtl, RT_IDR0 + 1);
	rtl->mac_addr[2] = RTL_READ_8(rtl, RT_IDR0 + 2);
	rtl->mac_addr[3] = RTL_READ_8(rtl, RT_IDR0 + 3);
  	rtl->mac_addr[4] = RTL_READ_8(rtl, RT_IDR0 + 4);
  	rtl->mac_addr[5] = RTL_READ_8(rtl, RT_IDR0 + 5);

  	dprintf("rtl8139: mac addr %x:%x:%x:%x:%x:%x\n",
  		rtl->mac_addr[0], rtl->mac_addr[1], rtl->mac_addr[2],
  		rtl->mac_addr[3], rtl->mac_addr[4], rtl->mac_addr[5]);

	// enable writing to the config registers
	RTL_WRITE_8(rtl, RT_CFG9346, 0xc0);

	// reset config 1
	RTL_WRITE_8(rtl, RT_CONFIG1, 0);

	// Enable receive and transmit functions
	RTL_WRITE_8(rtl, RT_CHIPCMD, RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE);

#if 1
	// Set Rx FIFO threashold to 1K, Rx size to  8k+16, 1024 byte DMA burst
	RTL_WRITE_32(rtl, RT_RXCONFIG, 0x0000ce00); /* ??? c680 -- WRAP mode? */
#else
	// Set Rx FIFO threashold to 1K, Rx size to 64k+16, 1024 byte DMA burst
	RTL_WRITE_32(rtl, RT_RXCONFIG, 0x0000de00);
#endif
	
	// Set Tx 1024 byte DMA burst
	RTL_WRITE_32(rtl, RT_TXCONFIG, 0x03000600);

	// Turn off lan-wake and set the driver-loaded bit
	RTL_WRITE_8(rtl, RT_CONFIG1, (RTL_READ_8(rtl, RT_CONFIG1) & ~0x30) | 0x20);

	// Enable FIFO auto-clear
	RTL_WRITE_8(rtl, RT_CONFIG4, RTL_READ_8(rtl, RT_CONFIG4) | 0x80);

	// go back to normal mode
	RTL_WRITE_8(rtl, RT_CFG9346, 0);

		// Setup RX buffers
	dprintf("rx buffer will be at 0x%lx\n", ADDR_RXBUF);
	RTL_WRITE_32(rtl, RT_RXBUF, ADDR_RXBUF);	
		// Setup TX buffers
	RTL_WRITE_32(rtl, RT_TXADDR0, ADDR_TXBUF0);
	RTL_WRITE_32(rtl, RT_TXADDR1, ADDR_TXBUF0 + 2*1024);
	RTL_WRITE_32(rtl, RT_TXADDR2, ADDR_TXBUF0 + 4*1024);
	RTL_WRITE_32(rtl, RT_TXADDR3, ADDR_TXBUF0 + 6*1024);
	
	// Reset RXMISSED counter
	RTL_WRITE_32(rtl, RT_RXMISSED, 0);

	// Enable receiving broadcast and physical match packets
//	RTL_WRITE_32(rtl, RT_RXCONFIG, RTL_READ_32(rtl, RT_RXCONFIG) | 0x0000000a);
	RTL_WRITE_32(rtl, RT_RXCONFIG, RTL_READ_32(rtl, RT_RXCONFIG) | 0x0000000f);

	// Filter out all multicast packets
	RTL_WRITE_32(rtl, RT_MAR0, 0);
	RTL_WRITE_32(rtl, RT_MAR0 + 4, 0);

	// Disable all multi-interrupts
	RTL_WRITE_16(rtl, RT_MULTIINTR, 0);

	RTL_WRITE_16(rtl, RT_INTRMASK, MYRT_INTS);
//	RTL_WRITE_16(rtl, RT_INTRMASK, 0x807f);

	// Enable RX/TX once more
	RTL_WRITE_8(rtl, RT_CHIPCMD, RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE);

	RTL_WRITE_8(rtl, RT_CFG9346, 0);

	return 0;

err1:
	vm_delete_region(vm_get_kernel_aspace_id(), rtl->region);
err:
	return err;
}

static void rtl8139_stop(rtl8139 *rtl)
{
	// stop the rx and tx and mask all interrupts
	RTL_WRITE_8(rtl, RT_CHIPCMD, RT_CMD_RESET);
	RTL_WRITE_16(rtl, RT_INTRMASK, 0);
}

static void rtl8139_resetrx(rtl8139 *rtl)
{
	rtl8139_stop(rtl);

	// reset the rx pointers
	RTL_WRITE_16(rtl, RT_RXBUFTAIL, 0);
	RTL_WRITE_16(rtl, RT_RXBUFHEAD, TAIL_TO_TAILREG(0));

	// start it back up
	RTL_WRITE_16(rtl, RT_INTRMASK, MYRT_INTS);

	// Enable RX/TX once more
	RTL_WRITE_8(rtl, RT_CHIPCMD, RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE);
}

void rtl8139_xmit(rtl8139 *rtl, const char *ptr, ssize_t len)
{
	int i;
	int txbn;
	int state;

restart:
	sem_acquire(rtl->tx_sem, 1);
	mutex_lock(&rtl->lock);

#if 0
	dprintf("XMIT %d %x (%d)\n",rtl->txbn, ptr, len);

	dprintf("dumping packet:");
	for(i=0; i<len; i++) {
		if(i%8 == 0)
			dprintf("\n");
		dprintf("0x%02x ", ptr[i]);
	}
	dprintf("\n");
#endif

	state = int_disable_interrupts();
	acquire_spinlock(&rtl->reg_spinlock);

	/* wait for clear-to-send */
	if(!(RTL_READ_32(rtl, RT_TXSTATUS0 + rtl->txbn*4) & RT_TX_HOST_OWNS)) {
		dprintf("rtl8139_xmit: no txbuf free\n");
		release_spinlock(&rtl->reg_spinlock);
		int_restore_interrupts(state);
		mutex_unlock(&rtl->lock);
		sem_release(rtl->tx_sem, 1);
		goto restart;
	}

	memcpy((void*)(rtl->txbuf + rtl->txbn * 0x800), ptr, len);
	if(len < 60)
		len = 60;

	RTL_WRITE_32(rtl, RT_TXSTATUS0 + rtl->txbn*4, len | 0x80000);
	if(++rtl->txbn >= 4)
		rtl->txbn = 0;

	release_spinlock(&rtl->reg_spinlock);
	int_restore_interrupts(state);

	mutex_unlock(&rtl->lock);
}

typedef struct rx_entry {
	volatile uint16 status;
	volatile uint16 len;
	volatile uint8 data[1];
} rx_entry;

ssize_t rtl8139_rx(rtl8139 *rtl, char *buf, ssize_t buf_len)
{
	rx_entry *entry;
	uint32 tail;
	uint16 len;
	int rc;
	int state;
	bool release_sem = false;

//	dprintf("rtl8139_rx: entry\n");

	if(buf_len < 1500)
		return -1;

restart:
	sem_acquire(rtl->rx_sem, 1);
	mutex_lock(&rtl->lock);

	state = int_disable_interrupts();
	acquire_spinlock(&rtl->reg_spinlock);

	tail = TAILREG_TO_TAIL(RTL_READ_16(rtl, RT_RXBUFTAIL));
//	dprintf("tailreg = 0x%x, actual tail 0x%x\n", RTL_READ_16(rtl, RT_RXBUFTAIL), tail);
	if(tail == RTL_READ_16(rtl, RT_RXBUFHEAD)) {
		release_spinlock(&rtl->reg_spinlock);
		int_restore_interrupts(state);
		mutex_unlock(&rtl->lock);
		goto restart;
	}

	if(RTL_READ_8(rtl, RT_CHIPCMD) & RT_CMD_RX_BUF_EMPTY) {
		release_spinlock(&rtl->reg_spinlock);
		int_restore_interrupts(state);
		mutex_unlock(&rtl->lock);
		goto restart;
	}

	// grab another buffer
	entry = (rx_entry *)((uint8 *)rtl->rxbuf + tail);
//	dprintf("entry->status = 0x%x\n", entry->status);
//	dprintf("entry->len = 0x%x\n", entry->len);

	// see if it's an unfinished buffer
	if(entry->len == 0xfff0) {
		release_spinlock(&rtl->reg_spinlock);
		int_restore_interrupts(state);
		mutex_unlock(&rtl->lock);
		goto restart;
	}

	// figure the len that we need to copy
	len = entry->len - 4; // minus the crc

	// see if we got an error
	if((entry->status & RT_RX_STATUS_OK) == 0 || len > 1500) {
		// error, lets reset the card
		rtl8139_resetrx(rtl);
		release_spinlock(&rtl->reg_spinlock);
		int_restore_interrupts(state);
		mutex_unlock(&rtl->lock);
		goto restart;
	}

	// copy the buffer
	if(len > buf_len) {
		dprintf("rtl8139_rx: packet too large for buffer (len %d, buf_len %d)\n", len, buf_len);
		RTL_WRITE_16(rtl, RT_RXBUFTAIL, TAILREG_TO_TAIL(RTL_READ_16(rtl, RT_RXBUFHEAD)));
		rc = ERR_TOO_BIG;
		release_sem = true;
		goto out;
	}
	if(tail + len > RXBUFMASK) {
		int pos = 0;

//		dprintf("packet wraps around\n");
		memcpy(buf, (const void *)&entry->data[0], RXBUFSIZE - (tail + 4));
		memcpy((uint8 *)buf + RXBUFSIZE - (tail + 4), (const void *)rtl->rxbuf, len - (RXBUFSIZE - (tail + 4)));
	} else {
		memcpy(buf, (const void *)&entry->data[0], len);
	}
	rc = len;

	// calculate the new tail
	tail = ((tail + entry->len + 4 + 3) & ~3) % RXBUFSIZE;
//	dprintf("new tail at 0x%x, tailreg will say 0x%x\n", tail, TAIL_TO_TAILREG(tail));
	RTL_WRITE_16(rtl, RT_RXBUFTAIL, TAIL_TO_TAILREG(tail));

	if(tail != RTL_READ_16(rtl, RT_RXBUFHEAD)) {
		// we're at last one more packet behind
		release_sem = true;
	}

out:
	release_spinlock(&rtl->reg_spinlock);
	int_restore_interrupts(state);

	if(release_sem)
		sem_release(rtl->rx_sem, 1);
	mutex_unlock(&rtl->lock);

	return rc;
}

static int rtl8139_rxint(rtl8139 *rtl, uint16 int_status)
{
	int rc = INT_NO_RESCHEDULE;

//	dprintf("rx\n");

//	dprintf("buf 0x%x, head 0x%x, tail 0x%x\n",
//		RTL_READ_32(rtl, RT_RXBUF), RTL_READ_16(rtl, RT_RXBUFHEAD), RTL_READ_16(rtl, RT_RXBUFTAIL));
//	dprintf("BUF_EMPTY = %d\n", RTL_READ_8(rtl, RT_CHIPCMD) & RT_CMD_RX_BUF_EMPTY);

	if(!(RTL_READ_8(rtl, RT_CHIPCMD) & RT_CMD_RX_BUF_EMPTY)) {
		sem_release_etc(rtl->rx_sem, 1, SEM_FLAG_NO_RESCHED);
		rc = INT_RESCHEDULE;
	}

	return rc;
}

static int rtl8139_txint(rtl8139 *rtl, uint16 int_status)
{
	uint32 txstat;
	int i;
	int rc = INT_NO_RESCHEDULE;

	// transmit ok
//	dprintf("tx\n");

	for(i=0; i<4; i++) {
		if(i > 0 && rtl->last_txbn == rtl->txbn)
			break;
		txstat = RTL_READ_32(rtl, RT_TXSTATUS0 + rtl->last_txbn*4);
//		dprintf("txstat[%d] = 0x%x\n", rtl->last_txbn, txstat);
		if(txstat & RT_TX_HOST_OWNS) {
//			dprintf("host now owns the buffer\n");
		} else {
//			dprintf("host no own\n");
			break;
		}
		if(++rtl->last_txbn >= 4)
			rtl->last_txbn = 0;
		sem_release_etc(rtl->tx_sem, 1, SEM_FLAG_NO_RESCHED);
		rc = INT_RESCHEDULE;
	}

	return rc;
}

static int rtl8139_int(void* data)
{
	int rc = INT_NO_RESCHEDULE;
	rtl8139 *rtl = (rtl8139 *)data;

	acquire_spinlock(&rtl->reg_spinlock);

	// Disable interrupts
	RTL_WRITE_16(rtl, RT_INTRMASK, 0);

	for(;;) {
		uint16 status = RTL_READ_16(rtl, RT_INTRSTATUS);
		
		if(status)
			RTL_WRITE_16(rtl, RT_INTRSTATUS, status);
		else
			break;

		if(status & RT_INT_TX_OK || status & RT_INT_TX_ERR) {
			if(rtl8139_txint(rtl, status) == INT_RESCHEDULE)
				rc = INT_RESCHEDULE;
		}
		if(status & RT_INT_RX_ERR || status & RT_INT_RX_OK) {
			if(rtl8139_rxint(rtl, status) == INT_RESCHEDULE)
				rc = INT_RESCHEDULE;
		}
		if(status & RT_INT_RXBUF_OVERFLOW) {
			dprintf("RX buffer overflow!\n");
			dprintf("buf 0x%x, head 0x%x, tail 0x%x\n",
				RTL_READ_32(rtl, RT_RXBUF), RTL_READ_16(rtl, RT_RXBUFHEAD), RTL_READ_16(rtl, RT_RXBUFTAIL));
			RTL_WRITE_32(rtl, RT_RXMISSED, 0);
			RTL_WRITE_16(rtl, RT_RXBUFTAIL, TAILREG_TO_TAIL(RTL_READ_16(rtl, RT_RXBUFHEAD)));
		}
		if(status & RT_INT_RXFIFO_OVERFLOW) {
			dprintf("RX fifo overflow!\n");
		}
		if(status & RT_INT_RXFIFO_UNDERRUN) {
			dprintf("RX fifo underrun\n");
		}
	}

	// reenable interrupts
	RTL_WRITE_16(rtl, RT_INTRMASK, MYRT_INTS);

	release_spinlock(&rtl->reg_spinlock);

	return rc;
}

