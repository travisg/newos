/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/vm.h>
#include <kernel/int.h>
#include <kernel/lock.h>
#include <kernel/sem.h>
#include <kernel/time.h>
#include <kernel/module.h>
#include <kernel/arch/cpu.h>
#include <kernel/arch/i386/cpu.h>
#include <kernel/net/ethernet.h>

#include <string.h>

#include <kernel/bus/pci/pci.h>

#include "ns83820_dev.h"
#include "ns83820_priv.h"

#define debug_level_flow 3
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "NS83820 -- "

#include <kernel/debug_ext.h>

#if 0
// macros to manipulate registers. Assumes a variable called ns of type ns83820 *
#define NS_WRITE_32(reg, dat) ({ \
	asm("wbinvd"); \
	*(vuint32 *)((ns)->virt_base + (reg)) = (dat); \
	dprintf("NS_WRITE_32: putting 0x%x at 0x%lx\n", dat, (ns)->virt_base + (reg)); \
	asm("wbinvd"); \
	})

#define NS_READ_32(reg) ({ \
	asm("wbinvd"); \
	uint32 temp = *(vuint32 *)((ns)->virt_base + (reg)); \
	dprintf("NS_READ_32: read 0x%x from 0x%lx\n", temp, (ns)->virt_base + (reg)); \
	temp; \
	})
#else
// macros to manipulate registers. Assumes a variable called ns of type ns83820 *
#define NS_WRITE_32(reg, dat) ({ \
	out32(dat, (ns)->io_port + (reg)); \
	dprintf("NS_WRITE_32: putting 0x%x at 0x%lx\n", dat, (ns)->io_port + (reg)); \
	})

#define NS_READ_32(reg) ({ \
	uint32 temp = in32((ns)->io_port + (reg)); \
	dprintf("NS_READ_32: read 0x%x from 0x%lx\n", temp, (ns)->io_port + (reg)); \
	temp; \
	})

#endif

int ns83820_detect(ns83820 **_ns, int *num)
{
	int i, j;
	pci_module_hooks *pci;
	pci_info pinfo;
	bool foundit = false;
	ns83820 *ns;
	int err;

	if(module_get(PCI_BUS_MODULE_NAME, 0, (void **)&pci) < 0) {
		dprintf("ns83820_detect: no pci bus found..\n");
		return -1;
	}

	j = 0;
	for(i = 0; pci->get_nth_pci_info(i, &pinfo) >= NO_ERROR; i++) {
		if(pinfo.vendor_id == NS_VENDOR_ID && pinfo.device_id == NS_DEVICE_ID_83820) {
			if(j == *num) {
				foundit = true;
				break;
			} else {
				j++;
			}
		}
	}
	if(!foundit) {
		dprintf("ns83820_detect: didn't find device on pci bus\n");
		err = ERR_NOT_FOUND;
		goto err;
	}

	// we found one
	dprintf("ns83820_detect: found device at pci %d:%d:%d\n", pinfo.bus, pinfo.device, pinfo.function);

	// increment the cookie that was passed to us so we wont find this device again
	(*num)++;

	ns = kmalloc(sizeof(ns83820));
	if(ns == NULL) {
		err = ERR_NO_MEMORY;
		goto err;
	}
	memset(ns, 0, sizeof(ns83820));
	ns->irq = pinfo.u.h0.interrupt_line;
	// find the memory-mapped base
	for(i=0; i<6; i++) {
		if(pinfo.u.h0.base_registers[i] > 0xffff) {
			ns->phys_base = pinfo.u.h0.base_registers[i];
			ns->phys_size = pinfo.u.h0.base_register_sizes[i];
			break;
		} else if(pinfo.u.h0.base_registers[i] > 0) {
			ns->io_port = pinfo.u.h0.base_registers[i];
		}
	}
	if(ns->phys_base == 0) {
		kfree(ns);
		err = ERR_IO_ERROR;
		goto err;
	}

	dprintf("detected ns83820 at irq %d, memory base 0x%lx, size 0x%lx, io base 0x%x\n", 
		ns->irq, ns->phys_base, ns->phys_size, ns->io_port);

	*_ns = ns;

	err = NO_ERROR;

err:
	module_put(PCI_BUS_MODULE_NAME);

	return err;
}

static int run_bist(ns83820 *ns, uint32 bist_enable_bit, uint32 bist_done_bit, uint32 bist_fail_bit)
{
	// start the bist
	NS_WRITE_32(REG_PTSCR, bist_enable_bit);

	// wait for it to complete or fail
	for(;;) {
		uint32 val = NS_READ_32(REG_PTSCR);

		if(val & bist_fail_bit)
			return -1;
		if(val & bist_done_bit)
			return 0;
		if((val & bist_enable_bit) == 0)
			return -1;

		thread_snooze(10);
	}
}

int ns83820_init(ns83820 *ns)
{
	bigtime_t time;
	int err = -1;
	addr_t temp;
	int i;

	SHOW_FLOW(1, "ns83820_init: ns %p", ns);

	ns->region = vm_map_physical_memory(vm_get_kernel_aspace_id(), "ns83820_init", (void **)&ns->virt_base,
		REGION_ADDR_ANY_ADDRESS, ns->phys_size, LOCK_KERNEL|LOCK_RW, ns->phys_base);
	if(ns->region < 0) {
		dprintf("ns83820_init: error creating memory mapped region\n");
		err = -1;
		goto err;
	}
	SHOW_FLOW(3, "ns83820 mapped at address 0x%lx", ns->virt_base);

	// reset the chip
	NS_WRITE_32(REG_CR, 0x100);
	while(NS_READ_32(REG_CR) & 0x100)
		;

	SHOW_FLOW0(3, "chip is reset");

	thread_snooze(100000);

	// run the BISTs
	NS_WRITE_32(REG_PTSCR, 1<<13); // RBIST_RST
	run_bist(ns, 1<<10, 1<<9, 0x1b8); // RBIST_EN
	run_bist(ns, 1<<1, 0, 1); // EEBIST_EN
	run_bist(ns, 1<<2, 0, 0); // EELOAD_EN

	// load the mac address
	for(i = 0; i < 3; i++) {
		uint16 temp;

		NS_WRITE_32(REG_RFCR, i * 2);
		temp = NS_READ_32(REG_RFDR);

		ns->mac_addr[i*2] = temp & 0xff;
		ns->mac_addr[i*2+1] = (temp >> 8) & 0xff;
	}

	SHOW_FLOW(1, "address %02x:%02x:%02x:%02x:%02x:%02x", 
		ns->mac_addr[0], ns->mac_addr[1], ns->mac_addr[2], 
		ns->mac_addr[3], ns->mac_addr[4], ns->mac_addr[5]);

	// set up device here
	return -1;

	return 0;

err1:
	vm_delete_region(vm_get_kernel_aspace_id(), ns->region);
err:
	return err;
}

static void ns83820_stop(ns83820 *ns)
{
#if 0
	// stop the rx and tx and mask all interrupts
	RTL_WRITE_8(rtl, RT_CHIPCMD, RT_CMD_RESET);
	RTL_WRITE_16(rtl, RT_INTRMASK, 0);
#endif
}

static void ns83820_resetrx(ns83820 *ns)
{
#if 0
	rtl8139_stop(rtl);

	// reset the rx pointers
	RTL_WRITE_16(rtl, RT_RXBUFTAIL, TAIL_TO_TAILREG(0));
	RTL_WRITE_16(rtl, RT_RXBUFHEAD, 0);

	// start it back up
	RTL_WRITE_16(rtl, RT_INTRMASK, MYRT_INTS);

	// Enable RX/TX once more
	RTL_WRITE_8(rtl, RT_CHIPCMD, RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE);
#endif
}

static void ns83820_dumptxstate(ns83820 *ns)
{
#if 0
	dprintf("tx state:\n");
	dprintf("\ttxbn %d\n", rtl->txbn);
	dprintf("\ttxstatus 0 0x%x\n", RTL_READ_32(rtl, RT_TXSTATUS0));
	dprintf("\ttxstatus 1 0x%x\n", RTL_READ_32(rtl, RT_TXSTATUS1));
	dprintf("\ttxstatus 2 0x%x\n", RTL_READ_32(rtl, RT_TXSTATUS2));
	dprintf("\ttxstatus 3 0x%x\n", RTL_READ_32(rtl, RT_TXSTATUS3));
#endif
}

void ns83820_xmit(ns83820 *ns, const char *ptr, ssize_t len)
{
#if 0
#if 0
	int i;
#endif

//restart:
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

	int_disable_interrupts();
	acquire_spinlock(&rtl->reg_spinlock);

#if 0
	/* wait for clear-to-send */
	if(!(RTL_READ_32(rtl, RT_TXSTATUS0 + rtl->txbn*4) & RT_TX_HOST_OWNS)) {
		dprintf("rtl8139_xmit: no txbuf free\n");
		rtl8139_dumptxstate(rtl);
		release_spinlock(&rtl->reg_spinlock);
		int_restore_interrupts();
		mutex_unlock(&rtl->lock);
		sem_release(rtl->tx_sem, 1);
		goto restart;
	}
#endif

	memcpy((void*)(rtl->txbuf + rtl->txbn * 0x800), ptr, len);
	if(len < ETHERNET_MIN_SIZE)
		len = ETHERNET_MIN_SIZE;

	RTL_WRITE_32(rtl, RT_TXSTATUS0 + rtl->txbn*4, len | 0x80000);
	if(++rtl->txbn >= 4)
		rtl->txbn = 0;

	release_spinlock(&rtl->reg_spinlock);
	int_restore_interrupts();

	mutex_unlock(&rtl->lock);
#endif
}

typedef struct rx_entry {
	volatile uint16 status;
	volatile uint16 len;
	volatile uint8 data[1];
} rx_entry;

ssize_t ns83820_rx(ns83820 *ns, char *buf, ssize_t buf_len)
{
#if 0
	rx_entry *entry;
	uint32 tail;
	uint16 len;
	int rc;
	bool release_sem = false;

//	dprintf("rtl8139_rx: entry\n");

	if(buf_len < 1500)
		return -1;

restart:
	sem_acquire(rtl->rx_sem, 1);
	mutex_lock(&rtl->lock);

	int_disable_interrupts();
	acquire_spinlock(&rtl->reg_spinlock);

	tail = TAILREG_TO_TAIL(RTL_READ_16(rtl, RT_RXBUFTAIL));
//	dprintf("tailreg = 0x%x, actual tail 0x%x\n", RTL_READ_16(rtl, RT_RXBUFTAIL), tail);
	if(tail == RTL_READ_16(rtl, RT_RXBUFHEAD)) {
		release_spinlock(&rtl->reg_spinlock);
		int_restore_interrupts();
		mutex_unlock(&rtl->lock);
		goto restart;
	}

	if(RTL_READ_8(rtl, RT_CHIPCMD) & RT_CMD_RX_BUF_EMPTY) {
		release_spinlock(&rtl->reg_spinlock);
		int_restore_interrupts();
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
		int_restore_interrupts();
		mutex_unlock(&rtl->lock);
		goto restart;
	}

	// figure the len that we need to copy
	len = entry->len - 4; // minus the crc

	// see if we got an error
	if((entry->status & RT_RX_STATUS_OK) == 0 || len > ETHERNET_MAX_SIZE) {
		// error, lets reset the card
		rtl8139_resetrx(rtl);
		release_spinlock(&rtl->reg_spinlock);
		int_restore_interrupts();
		mutex_unlock(&rtl->lock);
		goto restart;
	}

	// copy the buffer
	if(len > buf_len) {
		dprintf("rtl8139_rx: packet too large for buffer (len %d, buf_len %ld)\n", len, (long)buf_len);
		RTL_WRITE_16(rtl, RT_RXBUFTAIL, TAILREG_TO_TAIL(RTL_READ_16(rtl, RT_RXBUFHEAD)));
		rc = ERR_TOO_BIG;
		release_sem = true;
		goto out;
	}
	if(tail + len > 0xffff) {
//		dprintf("packet wraps around\n");
		memcpy(buf, (const void *)&entry->data[0], 0x10000 - (tail + 4));
		memcpy((uint8 *)buf + 0x10000 - (tail + 4), (const void *)rtl->rxbuf, len - (0x10000 - (tail + 4)));
	} else {
		memcpy(buf, (const void *)&entry->data[0], len);
	}
	rc = len;

	// calculate the new tail
	tail = ((tail + entry->len + 4 + 3) & ~3) % 0x10000;
//	dprintf("new tail at 0x%x, tailreg will say 0x%x\n", tail, TAIL_TO_TAILREG(tail));
	RTL_WRITE_16(rtl, RT_RXBUFTAIL, TAIL_TO_TAILREG(tail));

	if(tail != RTL_READ_16(rtl, RT_RXBUFHEAD)) {
		// we're at last one more packet behind
		release_sem = true;
	}

out:
	release_spinlock(&rtl->reg_spinlock);
	int_restore_interrupts();

	if(release_sem)
		sem_release(rtl->rx_sem, 1);
	mutex_unlock(&rtl->lock);

#if 0
{
	int i;
	dprintf("RX %x (%d)\n", buf, len);

	dprintf("dumping packet:");
	for(i=0; i<len; i++) {
		if(i%8 == 0)
			dprintf("\n");
		dprintf("0x%02x ", buf[i]);
	}
	dprintf("\n");
}
#endif

	return rc;
#endif
}

static int ns83820_rxint(ns83820 *ns, uint16 int_status)
{
#if 0
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
#endif
}

static int ns83820_txint(ns83820 *ns, uint16 int_status)
{
#if 0
	uint32 txstat;
	int i;
	int rc = INT_NO_RESCHEDULE;

	// transmit ok
//	dprintf("tx %d\n", int_status);
	if(int_status & RT_INT_TX_ERR) {
		dprintf("err tx int:\n");
		rtl8139_dumptxstate(rtl);
	}

	for(i=0; i<4; i++) {
		if(i > 0 && rtl->last_txbn == rtl->txbn)
			break;
		txstat = RTL_READ_32(rtl, RT_TXSTATUS0 + rtl->last_txbn*4);
//		dprintf("txstat[%d] = 0x%x\n", rtl->last_txbn, txstat);

		if((txstat & (RT_TX_STATUS_OK | RT_TX_UNDERRUN | RT_TX_ABORTED)) == 0)
			break;

		if(++rtl->last_txbn >= 4)
			rtl->last_txbn = 0;
		sem_release_etc(rtl->tx_sem, 1, SEM_FLAG_NO_RESCHED);
		rc = INT_RESCHEDULE;
	}

	return rc;
#endif
}

static int ns83820_int(void* data)
{
#if 0
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
			RTL_WRITE_16(rtl, RT_RXBUFTAIL, TAIL_TO_TAILREG(RTL_READ_16(rtl, RT_RXBUFHEAD)));
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
#endif
}

