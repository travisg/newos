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
#include <kernel/arch/i386/cpu.h>

#include <bus/bus.h>
#include <libc/string.h>

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

#define MYRT_INTS (RT_INT_PCIERR | RT_INT_RX_ERR | RT_INT_RX_OK | RT_INT_TX_ERR | RT_INT_TX_OK | RT_INT_RXBUF_OVERFLOW)

static rtl8139 *grtl;

static int rtl8139_int();

int rtl8139_detect(rtl8139 **rtl) 
{
	id_list *vendor_ids;
	id_list *device_ids;
	int err;
	device dev;
	int i;

	vendor_ids = kmalloc(sizeof(id_list) + sizeof(uint32));
	vendor_ids->num_ids = 1;
	vendor_ids->id[0] = RT_VENDORID;
	device_ids = kmalloc(sizeof(id_list) + sizeof(uint32));
	device_ids->num_ids = 1;
	device_ids->id[0] = RT_DEVICEID_8139;

	err = bus_find_device(1, vendor_ids, device_ids, &dev);
	if(err < 0) {
		err = -1;
		goto err;
	}
	
	// we found one
	dprintf("rtl8139_detect: found device at '%s'\n", dev.dev_path);

	*rtl = kmalloc(sizeof(rtl8139));
	if(*rtl == NULL) {
		err = -1;
		goto err;
	}
	memset(*rtl, 0, sizeof(rtl8139));
	(*rtl)->irq = dev.irq;
	// find the memory-mapped base
	for(i=0; i<MAX_DEV_IO_RANGES; i++) {
		if(dev.base[i] > 0xffff) {
			(*rtl)->phys_base = dev.base[i];
			(*rtl)->phys_size = dev.size[i];
			break;
		} else if(dev.base[i] > 0) {
			(*rtl)->io_port = dev.base[i];
		}
	}
	if((*rtl)->phys_base == 0) {
		kfree(*rtl);
		*rtl = NULL;
		err = -1;
		goto err;
	}

	dprintf("detected rtl8139 at irq %d, memory base 0x%x, size 0x%x\n", (*rtl)->irq, (*rtl)->phys_base, (*rtl)->phys_size);

err:
	kfree(vendor_ids);
	kfree(device_ids);

	return err;
}

int rtl8139_init(rtl8139 *rtl) 
{
	time_t time;
	int err = -1;
	addr temp;
	
	rtl->region = vm_map_physical_memory(vm_get_kernel_aspace_id(), "rtl8139_region", (void **)&rtl->virt_base,
		REGION_ADDR_ANY_ADDRESS, rtl->phys_size, LOCK_KERNEL|LOCK_RW, rtl->phys_base);
	if(rtl->region < 0) {
		dprintf("rtl8139_init: error creating memory mapped region\n");
		err = -1;
		goto err;
	}
	dprintf("rtl8139 mapped at address 0x%x\n", rtl->virt_base);

	// try to reset the device
 	time = system_time();
	RTL_WRITE_8(rtl, RT_CHIPCMD, RT_CMD_RESET);
	while((RTL_READ_8(rtl, RT_CHIPCMD) & RT_CMD_RESET)) {
		if(system_time() - time > 1000000) {
			err = -1;
			goto err1;
		}
	}

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

	// set up the interrupt handler
	grtl = rtl;
	int_set_io_interrupt_handler(rtl->irq + 0x20, &rtl8139_int);

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

	// Set Rx FIFO threashold to 1K, Rx size to 64k+16, 1024 byte DMA burst
	RTL_WRITE_32(rtl, RT_RXCONFIG, 0x0000de00);
	
	// Set Tx 1024 byte DMA burst
	RTL_WRITE_32(rtl, RT_TXCONFIG, 0x03000600);

	// Turn off lan-wake and set the driver-loaded bit
	RTL_WRITE_8(rtl, RT_CONFIG1, (RTL_READ_8(rtl, RT_CONFIG1) & ~0x30) | 0x20);
	
	// Enable FIFO auto-clear
	RTL_WRITE_8(rtl, RT_CONFIG4, RTL_READ_8(rtl, RT_CONFIG4) | 0x80);

	// go back to normal mode
	RTL_WRITE_8(rtl, RT_CFG9346, 0);

	// Setup RX buffers
	vm_get_page_mapping(vm_get_kernel_aspace(), rtl->rxbuf, &temp);
	RTL_WRITE_32(rtl, RT_RXBUF, temp);

	// Setup TX buffers
	vm_get_page_mapping(vm_get_kernel_aspace(), rtl->txbuf, &temp);
	RTL_WRITE_32(rtl, RT_TXADDR0, temp);
	RTL_WRITE_32(rtl, RT_TXADDR1, temp + 2*1024);
	vm_get_page_mapping(vm_get_kernel_aspace(), rtl->txbuf + 4*1024, &temp);
	RTL_WRITE_32(rtl, RT_TXADDR2, temp);
	RTL_WRITE_32(rtl, RT_TXADDR3, temp + 2*1024);
	
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

void rtl8139_xmit(rtl8139 *rtl, const char *ptr, int len)
{
	int i;
	int txbn;
	int state;

restart:
	sem_acquire(rtl->tx_sem, 1);
	mutex_lock(&rtl->lock);

//	dprintf("XMIT %d %x (%d)\n",rtl->txbn, ptr, len);

	/* wait for clear-to-send */
	if(!(RTL_READ_32(rtl, RT_TXSTATUS0 + rtl->txbn*4) & RT_TX_HOST_OWNS)) {
		dprintf("rtl8139_xmit: no txbuf free\n");
		mutex_unlock(&rtl->lock);
		sem_release(rtl->tx_sem, 1);
		goto restart;
	}	

	memcpy((void*)(rtl->txbuf + rtl->txbn * 0x800), ptr, len);
	if(len < 64) 
		len = 64;

	state = int_disable_interrupts();
	
	RTL_WRITE_32(rtl, RT_TXSTATUS0 + (rtl->txbn)*4, len | 0x80000);
	if(++rtl->txbn >= 4)
		rtl->txbn = 0;

	int_restore_interrupts(state);

	mutex_unlock(&rtl->lock);
}

typedef struct rx_entry {
	volatile uint16 status;
	volatile uint16 len;
	volatile uint8 data[1];
} rx_entry;

#define TAILREG_TO_TAIL(in) \
	(uint16)((uint16)(in) + (uint16)16)
#define TAIL_TO_TAILREG(in) \
	(uint16)((uint16)(in) - (uint16)16)

int rtl8139_rx(rtl8139 *rtl, char *buf, int buf_len)
{
	rx_entry *entry;
	uint32 tail;
	int rc;

//	dprintf("rtl8139_rx: entry\n");

	if(buf_len < 1500)
		return -1;

restart:
	sem_acquire(rtl->rx_sem, 1);
	mutex_lock(&rtl->lock);

	tail = TAILREG_TO_TAIL(RTL_READ_16(rtl, RT_RXBUFTAIL));
//	dprintf("tailreg = 0x%x, actual tail 0x%x\n", RTL_READ_16(rtl, RT_RXBUFTAIL), tail);
	if(tail == RTL_READ_16(rtl, RT_RXBUFHEAD)) {
		mutex_unlock(&rtl->lock);	
		goto restart;
	}

	// grab another buffer
	entry = (rx_entry *)((uint8 *)rtl->rxbuf + tail);
//	dprintf("entry->status = 0x%x\n", entry->status);
//	dprintf("entry->len = 0x%x\n", entry->len);

	// copy the buffer
	if(entry->len > buf_len) {
		dprintf("rtl8139_rx: packet too large for buffer\n");
		RTL_WRITE_16(grtl, RT_RXBUFTAIL, TAILREG_TO_TAIL(RTL_READ_16(grtl, RT_RXBUFHEAD)));
		mutex_unlock(&rtl->lock);
		sem_release(rtl->rx_sem, 1);
		return -1;
	}
	if(tail + entry->len > 0xffff) {
		int pos = 0;
		
		dprintf("packet wraps around\n");
		memcpy(buf, (const void *)&entry->data[0], 0x10000 - tail);
		memcpy((uint8 *)buf + 0x10000 - tail, (const void *)rtl->rxbuf, entry->len - (0x10000 - tail));
	} else {
		memcpy(buf, (const void *)&entry->data[0], entry->len);
	}
	rc = entry->len;	

	// calculate the new tail
	tail = (tail + entry->len + 4 + 3) & ~3;
	if(tail > 0xffff)
		tail = tail % 0x10000;
//	dprintf("new tail at 0x%x, tailreg will say 0x%x\n", tail, TAIL_TO_TAILREG(tail));
	RTL_WRITE_16(rtl, RT_RXBUFTAIL, TAIL_TO_TAILREG(tail));

	if(tail != RTL_READ_16(rtl, RT_RXBUFHEAD)) {
		// we're at last one more packet behind
		sem_release(rtl->rx_sem, 1);
	}
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
	}
	return rc;
}

static int rtl8139_txint(rtl8139 *rtl, uint16 int_status)
{
	uint32 txstat;
	int i;
	int rc = INT_NO_RESCHEDULE;

	// transmit ok
	dprintf("tx\n");

	for(i=0; i<4; i++) {
		if(grtl->last_txbn == grtl->txbn)
			break;
		txstat = RTL_READ_32(grtl, RT_TXSTATUS0 + grtl->last_txbn*4);
//		dprintf("txstat[%d] = 0x%x\n", grtl->last_txbn, txstat);
		if(txstat & RT_TX_HOST_OWNS) {
//			dprintf("host now owns the buffer\n");
		} else {
//			dprintf("host no own\n");
			break;
		}
		if(++grtl->last_txbn >= 4)
			grtl->last_txbn = 0;
		sem_release_etc(grtl->tx_sem, 1, SEM_FLAG_NO_RESCHED);
		rc = INT_RESCHEDULE;
	}
	return rc;
}

static int rtl8139_int()
{
	int rc = INT_NO_RESCHEDULE;
	
	// Disable interrupts
	RTL_WRITE_16(grtl, RT_INTRMASK, 0);

	for(;;) {
		uint16 status = RTL_READ_16(grtl, RT_INTRSTATUS);
		if(status)
			RTL_WRITE_16(grtl, RT_INTRSTATUS, status);
		else
			break;

		if(status & RT_INT_TX_OK || status & RT_INT_TX_ERR) {
			if(rtl8139_txint(grtl, status) == INT_RESCHEDULE)
				rc = INT_RESCHEDULE;
		}
		if(status & RT_INT_RX_ERR || status & RT_INT_RX_OK) {
			if(rtl8139_rxint(grtl, status) == INT_RESCHEDULE)
				rc = INT_RESCHEDULE;
		}
		if(status & RT_INT_RXBUF_OVERFLOW) {
			dprintf("RX buffer overflow!\n");
			dprintf("buf 0x%x, head 0x%x, tail 0x%x\n", 
				RTL_READ_32(grtl, RT_RXBUF), RTL_READ_16(grtl, RT_RXBUFHEAD), RTL_READ_16(grtl, RT_RXBUFTAIL));
			RTL_WRITE_32(grtl, RT_RXMISSED, 0);
			RTL_WRITE_16(grtl, RT_RXBUFTAIL, TAILREG_TO_TAIL(RTL_READ_16(grtl, RT_RXBUFHEAD)));
		}
		if(status & RT_INT_RXFIFO_OVERFLOW) {
			dprintf("RX fifo overflow!\n");
		}
		if(status & RT_INT_RXFIFO_UNDERRUN) {
			dprintf("RX fifo underrun\n");
		}
	}

	// reenable interrupts
	RTL_WRITE_16(grtl, RT_INTRMASK, MYRT_INTS);

	return rc;
}

