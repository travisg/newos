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
#include <kernel/module.h>
#include <kernel/arch/cpu.h>
#include <kernel/arch/i386/cpu.h>
#include <kernel/net/ethernet.h>

#include <string.h>

#include <kernel/bus/pci/pci.h>

#include "rtl8139_dev.h"
#include "rtl8139_priv.h"

#if 0
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
#else
#define RTL_WRITE_8(rtl, reg, dat) \
	out8(dat, (rtl)->io_port + (reg))
#define RTL_WRITE_16(rtl, reg, dat) \
	out16(dat, (rtl)->io_port + (reg))
#define RTL_WRITE_32(rtl, reg, dat) \
	out32(dat, (rtl)->io_port + (reg))

#define RTL_READ_8(rtl, reg) \
	in8((rtl)->io_port + (reg))
#define RTL_READ_16(rtl, reg) \
	in16((rtl)->io_port + (reg))
#define RTL_READ_32(rtl, reg) \
	in32((rtl)->io_port + (reg))
#endif

#define TAILREG_TO_TAIL(in) \
	(uint16)(((uint32)(in) + 16) % 0x10000)
#define TAIL_TO_TAILREG(in) \
	(uint16)((uint16)(in) - 16)

#define MYRT_INTS (RT_INT_PCIERR | RT_INT_RX_ERR | RT_INT_RX_OK | RT_INT_TX_ERR | RT_INT_TX_OK | RT_INT_RXBUF_OVERFLOW)

static int rtl8139_int(void*);

int rtl8139_detect(rtl8139 **rtl)
{
	int i;
	pci_module_hooks *pci;
	pci_info pinfo;
	bool foundit = false;

	if(module_get(PCI_BUS_MODULE_NAME, 0, (void **)&pci) < 0) {
		dprintf("rtl8139_detect: no pci bus found..\n");
		return -1;
	}

	for(i = 0; pci->get_nth_pci_info(i, &pinfo) >= NO_ERROR; i++) {
		if(pinfo.vendor_id == RT_VENDORID && pinfo.device_id == RT_DEVICEID_8139) {
			foundit = true;
			break;
		}
	}
	if(!foundit) {
		dprintf("rtl8139_detect: didn't find device on pci bus\n");
		return -1;
	}

	// we found one
	dprintf("rtl8139_detect: found device at pci %d:%d:%d\n", pinfo.bus, pinfo.device, pinfo.function);

	*rtl = kmalloc(sizeof(rtl8139));
	if(*rtl == NULL) {
		return ERR_NO_MEMORY;
	}
	memset(*rtl, 0, sizeof(rtl8139));
	(*rtl)->irq = pinfo.u.h0.interrupt_line;
	// find the memory-mapped base
	for(i=0; i<6; i++) {
		if(pinfo.u.h0.base_registers[i] > 0xffff) {
			(*rtl)->phys_base = pinfo.u.h0.base_registers[i];
			(*rtl)->phys_size = pinfo.u.h0.base_register_sizes[i];
			break;
		} else if(pinfo.u.h0.base_registers[i] > 0) {
			(*rtl)->io_port = pinfo.u.h0.base_registers[i];
		}
	}
	if((*rtl)->phys_base == 0) {
		kfree(*rtl);
		*rtl = NULL;
		return -1;
	}

	dprintf("detected rtl8139 at irq %d, memory base 0x%lx, size 0x%lx\n", (*rtl)->irq, (*rtl)->phys_base, (*rtl)->phys_size);

	return 0;
}

int rtl8139_init(rtl8139 *rtl)
{
	bigtime_t time;
	int err = -1;
	addr_t temp;

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
			goto err1;
		}
	} while((RTL_READ_8(rtl, RT_CHIPCMD) & RT_CMD_RESET));

	// create a rx and tx buf
	rtl->rxbuf_region = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "rtl8139_rxbuf", (void **)&rtl->rxbuf,
		REGION_ADDR_ANY_ADDRESS, 64*1024 + 16, REGION_WIRING_WIRED_CONTIG, LOCK_KERNEL|LOCK_RW);
	rtl->txbuf_region = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "rtl8139_txbuf", (void **)&rtl->txbuf,
		REGION_ADDR_ANY_ADDRESS, 8*1024, REGION_WIRING_WIRED, LOCK_KERNEL|LOCK_RW);

	// set up the transmission buf and sem
	rtl->tx_sem = sem_create(4, "rtl8139_txsem");
	mutex_init(&rtl->lock, "rtl8139");
	rtl->txbn = 0;
	rtl->last_txbn = 0;
	rtl->rx_sem = sem_create(0, "rtl8139_rxsem");
	rtl->reg_spinlock = 0;

	// set up the interrupt handler
	int_set_io_interrupt_handler(rtl->irq + 0x20, &rtl8139_int, rtl);

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

	// Set Rx FIFO threashold to 256, Rx size to 64k+16, 256 byte DMA burst
	RTL_WRITE_32(rtl, RT_RXCONFIG, 0x00009c00);

	// Set Tx 256 byte DMA burst
	RTL_WRITE_32(rtl, RT_TXCONFIG, 0x03000400);

	// Turn off lan-wake and set the driver-loaded bit
	RTL_WRITE_8(rtl, RT_CONFIG1, (RTL_READ_8(rtl, RT_CONFIG1) & ~0x30) | 0x20);

	// Enable FIFO auto-clear
	RTL_WRITE_8(rtl, RT_CONFIG4, RTL_READ_8(rtl, RT_CONFIG4) | 0x80);

	// go back to normal mode
	RTL_WRITE_8(rtl, RT_CFG9346, 0);

	// Setup RX buffers
	*(int *)rtl->rxbuf = 0;
	vm_get_page_mapping(vm_get_kernel_aspace_id(), rtl->rxbuf, &temp);
	dprintf("rx buffer will be at 0x%lx\n", temp);
	RTL_WRITE_32(rtl, RT_RXBUF, temp);

	// Setup TX buffers
	dprintf("tx buffer (virtual) is at 0x%lx\n", rtl->txbuf);
	*(int *)rtl->txbuf = 0;
	vm_get_page_mapping(vm_get_kernel_aspace_id(), rtl->txbuf, &temp);
	RTL_WRITE_32(rtl, RT_TXADDR0, temp);
	RTL_WRITE_32(rtl, RT_TXADDR1, temp + 2*1024);
	dprintf("first half of txbuf at 0x%lx\n", temp);
	*(int *)(rtl->txbuf + 4*1024) = 0;
	vm_get_page_mapping(vm_get_kernel_aspace_id(), rtl->txbuf + 4*1024, &temp);
	RTL_WRITE_32(rtl, RT_TXADDR2, temp);
	RTL_WRITE_32(rtl, RT_TXADDR3, temp + 2*1024);
	dprintf("second half of txbuf at 0x%lx\n", temp);

/*
	RTL_WRITE_32(rtl, RT_TXSTATUS0, RTL_READ_32(rtl, RT_TXSTATUS0) | 0xfffff000);
	RTL_WRITE_32(rtl, RT_TXSTATUS1, RTL_READ_32(rtl, RT_TXSTATUS1) | 0xfffff000);
	RTL_WRITE_32(rtl, RT_TXSTATUS2, RTL_READ_32(rtl, RT_TXSTATUS2) | 0xfffff000);
	RTL_WRITE_32(rtl, RT_TXSTATUS3, RTL_READ_32(rtl, RT_TXSTATUS3) | 0xfffff000);
*/
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
	RTL_WRITE_16(rtl, RT_RXBUFTAIL, TAIL_TO_TAILREG(0));
	RTL_WRITE_16(rtl, RT_RXBUFHEAD, 0);

	// start it back up
	RTL_WRITE_16(rtl, RT_INTRMASK, MYRT_INTS);

	// Enable RX/TX once more
	RTL_WRITE_8(rtl, RT_CHIPCMD, RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE);
}

static void rtl8139_dumptxstate(rtl8139 *rtl)
{
	dprintf("tx state:\n");
	dprintf("\ttxbn %d\n", rtl->txbn);
	dprintf("\ttxstatus 0 0x%x\n", RTL_READ_32(rtl, RT_TXSTATUS0));
	dprintf("\ttxstatus 1 0x%x\n", RTL_READ_32(rtl, RT_TXSTATUS1));
	dprintf("\ttxstatus 2 0x%x\n", RTL_READ_32(rtl, RT_TXSTATUS2));
	dprintf("\ttxstatus 3 0x%x\n", RTL_READ_32(rtl, RT_TXSTATUS3));
}

void rtl8139_xmit(rtl8139 *rtl, const char *ptr, ssize_t len)
{
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
}

