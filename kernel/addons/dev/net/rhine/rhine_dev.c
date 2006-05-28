/*
** Copyright 2001-2006, Travis Geiselbrecht. All rights reserved.
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

#include "rhine_dev.h"
#include "rhine_priv.h"

#if 0
#define RHINE_WRITE_8(r, reg, dat) \
	*(volatile uint8 *)((r)->virt_base + (reg)) = (dat)
#define RHINE_WRITE_16(r, reg, dat) \
	*(volatile uint16 *)((r)->virt_base + (reg)) = (dat)
#define RHINE_WRITE_32(r, reg, dat) \
	*(volatile uint32 *)((r)->virt_base + (reg)) = (dat)

#define RHINE_READ_8(r, reg) \
	*(volatile uint8 *)((r)->virt_base + reg)
#define RHINE_READ_16(r, reg) \
	*(volatile uint16 *)((r)->virt_base + reg)
#define RHINE_READ_32(r, reg) \
	*(volatile uint32 *)((r)->virt_base + reg)
#else
#define RHINE_WRITE_8(r, reg, dat) \
	out8(dat, (r)->io_port + (reg))
#define RHINE_WRITE_16(r, reg, dat) \
	out16(dat, (r)->io_port + (reg))
#define RHINE_WRITE_32(r, reg, dat) \
	out32(dat, (r)->io_port + (reg))

#define RHINE_READ_8(r, reg) \
	in8((r)->io_port + (reg))
#define RHINE_READ_16(r, reg) \
	in16((r)->io_port + (reg))
#define RHINE_READ_32(r, reg) \
	in32((r)->io_port + (reg))
#endif

#define RHINE_SETBITS_8(r, reg, bits) \
	RHINE_WRITE_8(r, reg, RHINE_READ_8(r, reg) | (bits))
#define RHINE_SETBITS_16(r, reg, bits) \
	RHINE_WRITE_16(r, reg, RHINE_READ_16(r, reg) | (bits))
#define RHINE_SETBITS_32(r, reg, bits) \
	RHINE_WRITE_32(r, reg, RHINE_READ_32(r, reg) | (bits))
#define RHINE_CLRBITS_8(r, reg, bits) \
	RHINE_WRITE_8(r, reg, RHINE_READ_8(r, reg) | ~(bits))
#define RHINE_CLRBITS_16(r, reg, bits) \
	RHINE_WRITE_16(r, reg, RHINE_READ_16(r, reg) | ~(bits))
#define RHINE_CLRBITS_32(r, reg, bits) \
	RHINE_WRITE_32(r, reg, RHINE_READ_32(r, reg) | ~(bits))

#define RXDESC(r, num) (r)->rxdesc[num]
#define TXDESC(r, num) (r)->txdesc[num]
#define RXDESC_PHYS(r, num) ((r)->rxdesc_phys + (num) * sizeof(struct rhine_rx_desc))
#define TXDESC_PHYS(r, num) ((r)->txdesc_phys + (num) * sizeof(struct rhine_tx_desc))

static int rhine_int(void*);

struct vendor_dev_match {
	int vendor;
	int device;
};
static const struct vendor_dev_match match[] = {
	{ 0x1106, 0x3043 },
	{ 0x1106, 0x6100 },
	{ 0x1106, 0x3065 },
	{ 0x1106, 0x3106 },
	{ 0x1106, 0x3053 }
};

static addr_t vtophys(void *virt)
{
	addr_t phys = 0;

	vm_get_page_mapping(vm_get_kernel_aspace_id(), (addr_t)virt, &phys);
	return phys;
}

int rhine_detect(rhine **rhine_list)
{
	int i, j;
	pci_module_hooks *pci;
	pci_info pinfo;
	rhine *r;

	*rhine_list = NULL;
	if(module_get(PCI_BUS_MODULE_NAME, 0, (void **)&pci) < 0) {
		dprintf("rhine_detect: no pci bus found..\n");
		return -1;
	}

	for (i = 0; pci->get_nth_pci_info(i, &pinfo) >= NO_ERROR; i++) {
		for (j = 0; j < sizeof(match)/sizeof(match[0]); j++) {
			if (pinfo.vendor_id == match[j].vendor && pinfo.device_id == match[j].device) {
				// we found one
				dprintf("rhine_detect: found device at pci %d:%d:%d\n", pinfo.bus, pinfo.device, pinfo.function);

				r = kmalloc(sizeof(rhine));
				if (r == NULL) {
					dprintf("rhine_detect: error allocating memory for rhine structure\n");
					continue;
				}

				memset(r, 0, sizeof(rhine));
				r->irq = pinfo.u.h0.interrupt_line;
				// find the memory-mapped base
				int range;
				for (range = 0; range < 6; range++) {
					if (pinfo.u.h0.base_registers[range] > 0xffff) {
						r->phys_base = pinfo.u.h0.base_registers[range];
						r->phys_size = pinfo.u.h0.base_register_sizes[range];
						break;
					} else if (pinfo.u.h0.base_registers[range] > 0) {
						r->io_port = pinfo.u.h0.base_registers[range];
					}
				}
				if (r->phys_base == 0) {
					kfree(r);
					r = NULL;
					continue;
				}

				dprintf("detected rhine at irq %d, memory base 0x%lx, size 0x%lx, io base 0x%lx\n", r->irq, r->phys_base, r->phys_size, r->io_port);

				// add it to the list
				r->next = *rhine_list;
				*rhine_list = r;
			}
		}
	}

	module_put(PCI_BUS_MODULE_NAME);

	return *rhine_list ? 0 : ERR_NOT_FOUND;
}

int rhine_init(rhine *r)
{
	bigtime_t time;
	int err = -1;
	addr_t temp;
	int i;

	dprintf("rhine_init: r %p\n", r);

	r->region = vm_map_physical_memory(vm_get_kernel_aspace_id(), "rhine_region", (void **)&r->virt_base,
		REGION_ADDR_ANY_ADDRESS, r->phys_size, LOCK_KERNEL|LOCK_RW, r->phys_base);
	if(r->region < 0) {
		dprintf("rhine_init: error creating memory mapped region\n");
		err = -1;
		goto err;
	}
	dprintf("rhine mapped at address 0x%lx\n", r->virt_base);

	/* create regions for tx and rx descriptors */
	r->rxdesc_region = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "rhine_rxdesc", (void **)&r->rxdesc,
		REGION_ADDR_ANY_ADDRESS, RXDESC_COUNT * sizeof(struct rhine_rx_desc), REGION_WIRING_WIRED_CONTIG, LOCK_KERNEL|LOCK_RW);
	r->rxdesc_phys = vtophys(r->rxdesc);
	dprintf("rhine: rx descriptors at %p, phys 0x%x\n", r->rxdesc, r->rxdesc_phys);
	r->txdesc_region = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "rhine_txdesc", (void **)&r->txdesc,
		REGION_ADDR_ANY_ADDRESS, TXDESC_COUNT * sizeof(struct rhine_tx_desc), REGION_WIRING_WIRED_CONTIG, LOCK_KERNEL|LOCK_RW);
	r->txdesc_phys = vtophys(r->txdesc);
	dprintf("rhine: tx descriptors at %p, phys 0x%x\n", r->txdesc, r->txdesc_phys);
	r->reg_spinlock = 0;

	/* stick all rx and tx buffers in a circular buffer */
	for (i=0; i < RXDESC_COUNT; i++) {
		RXDESC(r, i).status = 0;
		RXDESC(r, i).framelen = 0;
		RXDESC(r, i).buflen = 0;
		RXDESC(r, i).ptr = 0;
		if (i == RXDESC_COUNT-1)
			RXDESC(r, i).next = RXDESC_PHYS(r, 0);
		else
			RXDESC(r, i).next = RXDESC_PHYS(r, i + 1);
	}
	// XXX do same for tx


	r->rx_head = r->rx_tail = 0;

	/* reset the chip */
	time = system_time();
	RHINE_WRITE_16(r, RHINE_CR0, 0x8000); // reset the chip
	do {
		thread_snooze(10000); // 10ms
		if(system_time() - time > 1000000) {
			break;
		}
	} while(RHINE_READ_16(r, RHINE_CR0) & 0x8000);

	if (RHINE_READ_16(r, RHINE_CR0) & 0x8000) {
		dprintf("chip didn't reset, trying alternate method\n");
		RHINE_SETBITS_8(r, RHINE_MISC_CR1, 0x40);
		thread_snooze(10000);
	}

	/* read in the mac address */
	RHINE_WRITE_8(r, RHINE_EECSR, RHINE_READ_8(r, RHINE_EECSR) | (1<<5));
	r->mac_addr[0] = RHINE_READ_8(r, RHINE_PAR0); 
	r->mac_addr[1] = RHINE_READ_8(r, RHINE_PAR1);
   	r->mac_addr[2] = RHINE_READ_8(r, RHINE_PAR2);
	r->mac_addr[3] = RHINE_READ_8(r, RHINE_PAR3);
   	r->mac_addr[4] = RHINE_READ_8(r, RHINE_PAR4);
   	r->mac_addr[5] = RHINE_READ_8(r, RHINE_PAR5);
  	dprintf("rhine: mac addr %x:%x:%x:%x:%x:%x\n",
  		r->mac_addr[0], r->mac_addr[1], r->mac_addr[2],
  		r->mac_addr[3], r->mac_addr[4], r->mac_addr[5]);

	/* set up the rx state */
	/* 64 byte fifo threshold, all physical/broadcast/multicast/small/error packets accepted */
	RHINE_WRITE_8(r, RHINE_RCR, (0<<5) | (1<<4) | (1<<3) | (1<<2) | (1<<1) | (1<<0));
	RHINE_WRITE_32(r, RHINE_RDA0, RXDESC_PHYS(r, r->rx_head));

	/* set up tx state */
	/* 64 byte fifo, default backup, default loopback mode */
	RHINE_WRITE_8(r, RHINE_TCR, 0);

	/* mask all interrupts */
	RHINE_WRITE_16(r, RHINE_IMR0, 0);

	/* clear all pending interrupts */
	RHINE_WRITE_16(r, RHINE_ISR0, 0xffff);
	
	/* set up the interrupt handler */
	int_set_io_interrupt_handler(r->irq, &rhine_int, r, "rhine");

	{
		static uint8 buf[2048];
		RXDESC(r, r->rx_tail).ptr = vtophys(buf);
		RXDESC(r, r->rx_tail).buflen = sizeof(buf);
		RXDESC(r, r->rx_tail).status = 0;
		RXDESC(r, r->rx_tail).framelen = RHINE_RX_OWNER;
		r->rx_tail++;

		RHINE_WRITE_16(r, RHINE_CR0, (1<<1) | (1<<3) | (1<<6));
	}

	/* unmask all interrupts */
	RHINE_WRITE_16(r, RHINE_IMR0, 0xffff);	

#if 0
	// try to reset the device
 	time = system_time();
	RTL_WRITE_8(r, RT_CHIPCMD, RT_CMD_RESET);
	do {
		thread_snooze(10000); // 10ms
		if(system_time() - time > 1000000) {
			err = -1;
			goto err1;
		}
	} while((RTL_READ_8(r, RT_CHIPCMD) & RT_CMD_RESET));

	// create a rx and tx buf
	r->rxbuf_region = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "rhine_rxbuf", (void **)&r->rxbuf,
		REGION_ADDR_ANY_ADDRESS, 64*1024 + 16, REGION_WIRING_WIRED_CONTIG, LOCK_KERNEL|LOCK_RW);
	r->txbuf_region = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "rhine_txbuf", (void **)&r->txbuf,
		REGION_ADDR_ANY_ADDRESS, 8*1024, REGION_WIRING_WIRED, LOCK_KERNEL|LOCK_RW);

	// set up the transmission buf and sem
	r->tx_sem = sem_create(4, "rhine_txsem");
	mutex_init(&r->lock, "rhine");
	r->txbn = 0;
	r->last_txbn = 0;
	r->rx_sem = sem_create(0, "rhine_rxsem");
	r->reg_spinlock = 0;

	// set up the interrupt handler
	int_set_io_interrupt_handler(r->irq, &rhine_int, r, "rhine");

	// read the mac address
	r->mac_addr[0] = RTL_READ_8(r, RT_IDR0);
	r->mac_addr[1] = RTL_READ_8(r, RT_IDR0 + 1);
	r->mac_addr[2] = RTL_READ_8(r, RT_IDR0 + 2);
	r->mac_addr[3] = RTL_READ_8(r, RT_IDR0 + 3);
  	r->mac_addr[4] = RTL_READ_8(r, RT_IDR0 + 4);
  	r->mac_addr[5] = RTL_READ_8(r, RT_IDR0 + 5);

  	dprintf("rhine: mac addr %x:%x:%x:%x:%x:%x\n",
  		r->mac_addr[0], r->mac_addr[1], r->mac_addr[2],
  		r->mac_addr[3], r->mac_addr[4], r->mac_addr[5]);

	// enable writing to the config registers
	RTL_WRITE_8(r, RT_CFG9346, 0xc0);

	// reset config 1
	RTL_WRITE_8(r, RT_CONFIG1, 0);

	// Enable receive and transmit functions
	RTL_WRITE_8(r, RT_CHIPCMD, RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE);

	// Set Rx FIFO threashold to 256, Rx size to 64k+16, 256 byte DMA burst
	RTL_WRITE_32(r, RT_RXCONFIG, 0x00009c00);

	// Set Tx 256 byte DMA burst
	RTL_WRITE_32(r, RT_TXCONFIG, 0x03000400);

	// Turn off lan-wake and set the driver-loaded bit
	RTL_WRITE_8(r, RT_CONFIG1, (RTL_READ_8(r, RT_CONFIG1) & ~0x30) | 0x20);

	// Enable FIFO auto-clear
	RTL_WRITE_8(r, RT_CONFIG4, RTL_READ_8(r, RT_CONFIG4) | 0x80);

	// go back to normal mode
	RTL_WRITE_8(r, RT_CFG9346, 0);

	// Setup RX buffers
	*(int *)r->rxbuf = 0;
	vm_get_page_mapping(vm_get_kernel_aspace_id(), r->rxbuf, &temp);
	dprintf("rx buffer will be at 0x%lx\n", temp);
	RTL_WRITE_32(r, RT_RXBUF, temp);

	// Setup TX buffers
	dprintf("tx buffer (virtual) is at 0x%lx\n", r->txbuf);
	*(int *)r->txbuf = 0;
	vm_get_page_mapping(vm_get_kernel_aspace_id(), r->txbuf, &temp);
	RTL_WRITE_32(r, RT_TXADDR0, temp);
	RTL_WRITE_32(r, RT_TXADDR1, temp + 2*1024);
	dprintf("first half of txbuf at 0x%lx\n", temp);
	*(int *)(r->txbuf + 4*1024) = 0;
	vm_get_page_mapping(vm_get_kernel_aspace_id(), r->txbuf + 4*1024, &temp);
	RTL_WRITE_32(r, RT_TXADDR2, temp);
	RTL_WRITE_32(r, RT_TXADDR3, temp + 2*1024);
	dprintf("second half of txbuf at 0x%lx\n", temp);

/*
	RTL_WRITE_32(r, RT_TXSTATUS0, RTL_READ_32(r, RT_TXSTATUS0) | 0xfffff000);
	RTL_WRITE_32(r, RT_TXSTATUS1, RTL_READ_32(r, RT_TXSTATUS1) | 0xfffff000);
	RTL_WRITE_32(r, RT_TXSTATUS2, RTL_READ_32(r, RT_TXSTATUS2) | 0xfffff000);
	RTL_WRITE_32(r, RT_TXSTATUS3, RTL_READ_32(r, RT_TXSTATUS3) | 0xfffff000);
*/
	// Reset RXMISSED counter
	RTL_WRITE_32(r, RT_RXMISSED, 0);

	// Enable receiving broadcast and physical match packets
//	RTL_WRITE_32(r, RT_RXCONFIG, RTL_READ_32(r, RT_RXCONFIG) | 0x0000000a);
	RTL_WRITE_32(r, RT_RXCONFIG, RTL_READ_32(r, RT_RXCONFIG) | 0x0000000f);

	// Filter out all multicast packets
	RTL_WRITE_32(r, RT_MAR0, 0);
	RTL_WRITE_32(r, RT_MAR0 + 4, 0);

	// Disable all multi-interrupts
	RTL_WRITE_16(r, RT_MULTIINTR, 0);

	RTL_WRITE_16(r, RT_INTRMASK, MYRT_INTS);
//	RTL_WRITE_16(r, RT_INTRMASK, 0x807f);

	// Enable RX/TX once more
	RTL_WRITE_8(r, RT_CHIPCMD, RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE);

	RTL_WRITE_8(r, RT_CFG9346, 0);
#endif

	return 0;

err1:
	vm_delete_region(vm_get_kernel_aspace_id(), r->region);
err:
	return err;
}

static void rhine_stop(rhine *r)
{
	PANIC_UNIMPLEMENTED();
#if 0
	// stop the rx and tx and mask all interrupts
	RTL_WRITE_8(r, RT_CHIPCMD, RT_CMD_RESET);
	RTL_WRITE_16(r, RT_INTRMASK, 0);
#endif
}

static void rhine_resetrx(rhine *r)
{
	PANIC_UNIMPLEMENTED();
#if 0
	rhine_stop(r);

	// reset the rx pointers
	RTL_WRITE_16(r, RT_RXBUFTAIL, TAIL_TO_TAILREG(0));
	RTL_WRITE_16(r, RT_RXBUFHEAD, 0);

	// start it back up
	RTL_WRITE_16(r, RT_INTRMASK, MYRT_INTS);

	// Enable RX/TX once more
	RTL_WRITE_8(r, RT_CHIPCMD, RT_CMD_RX_ENABLE | RT_CMD_TX_ENABLE);
#endif
}

static void rhine_dumptxstate(rhine *r)
{
	PANIC_UNIMPLEMENTED();
#if 0
	dprintf("tx state:\n");
	dprintf("\ttxbn %d\n", r->txbn);
	dprintf("\ttxstatus 0 0x%x\n", RTL_READ_32(r, RT_TXSTATUS0));
	dprintf("\ttxstatus 1 0x%x\n", RTL_READ_32(r, RT_TXSTATUS1));
	dprintf("\ttxstatus 2 0x%x\n", RTL_READ_32(r, RT_TXSTATUS2));
	dprintf("\ttxstatus 3 0x%x\n", RTL_READ_32(r, RT_TXSTATUS3));
#endif
}

void rhine_xmit(rhine *r, const char *ptr, ssize_t len)
{
#if 0
	PANIC_UNIMPLEMENTED();
#if 0
	int i;
#endif

//restart:
	sem_acquire(r->tx_sem, 1);
	mutex_lock(&r->lock);

#if 0
	dprintf("XMIT %d %x (%d)\n",r->txbn, ptr, len);

	dprintf("dumping packet:");
	for(i=0; i<len; i++) {
		if(i%8 == 0)
			dprintf("\n");
		dprintf("0x%02x ", ptr[i]);
	}
	dprintf("\n");
#endif

	int_disable_interrupts();
	acquire_spinlock(&r->reg_spinlock);

#if 0
	/* wait for clear-to-send */
	if(!(RTL_READ_32(r, RT_TXSTATUS0 + r->txbn*4) & RT_TX_HOST_OWNS)) {
		dprintf("rhine_xmit: no txbuf free\n");
		rhine_dumptxstate(r);
		release_spinlock(&r->reg_spinlock);
		int_restore_interrupts();
		mutex_unlock(&r->lock);
		sem_release(r->tx_sem, 1);
		goto restart;
	}
#endif

	memcpy((void*)(r->txbuf + r->txbn * 0x800), ptr, len);
	if(len < ETHERNET_MIN_SIZE)
		len = ETHERNET_MIN_SIZE;

	RTL_WRITE_32(r, RT_TXSTATUS0 + r->txbn*4, len | 0x80000);
	if(++r->txbn >= 4)
		r->txbn = 0;

	release_spinlock(&r->reg_spinlock);
	int_restore_interrupts();

	mutex_unlock(&r->lock);
#endif
}

typedef struct rx_entry {
	volatile uint16 status;
	volatile uint16 len;
	volatile uint8 data[1];
} rx_entry;

ssize_t rhine_rx(rhine *r, char *buf, ssize_t buf_len)
{
	PANIC_UNIMPLEMENTED();
#if 0
	rx_entry *entry;
	uint32 tail;
	uint16 len;
	int rc;
	bool release_sem = false;

//	dprintf("rhine_rx: entry\n");

	if(buf_len < 1500)
		return -1;

restart:
	sem_acquire(r->rx_sem, 1);
	mutex_lock(&r->lock);

	int_disable_interrupts();
	acquire_spinlock(&r->reg_spinlock);

	tail = TAILREG_TO_TAIL(RTL_READ_16(r, RT_RXBUFTAIL));
//	dprintf("tailreg = 0x%x, actual tail 0x%x\n", RTL_READ_16(r, RT_RXBUFTAIL), tail);
	if(tail == RTL_READ_16(r, RT_RXBUFHEAD)) {
		release_spinlock(&r->reg_spinlock);
		int_restore_interrupts();
		mutex_unlock(&r->lock);
		goto restart;
	}

	if(RTL_READ_8(r, RT_CHIPCMD) & RT_CMD_RX_BUF_EMPTY) {
		release_spinlock(&r->reg_spinlock);
		int_restore_interrupts();
		mutex_unlock(&r->lock);
		goto restart;
	}

	// grab another buffer
	entry = (rx_entry *)((uint8 *)r->rxbuf + tail);
//	dprintf("entry->status = 0x%x\n", entry->status);
//	dprintf("entry->len = 0x%x\n", entry->len);

	// see if it's an unfinished buffer
	if(entry->len == 0xfff0) {
		release_spinlock(&r->reg_spinlock);
		int_restore_interrupts();
		mutex_unlock(&r->lock);
		goto restart;
	}

	// figure the len that we need to copy
	len = entry->len - 4; // minus the crc

	// see if we got an error
	if((entry->status & RT_RX_STATUS_OK) == 0 || len > ETHERNET_MAX_SIZE) {
		// error, lets reset the card
		rhine_resetrx(r);
		release_spinlock(&r->reg_spinlock);
		int_restore_interrupts();
		mutex_unlock(&r->lock);
		goto restart;
	}

	// copy the buffer
	if(len > buf_len) {
		dprintf("rhine_rx: packet too large for buffer (len %d, buf_len %ld)\n", len, (long)buf_len);
		RTL_WRITE_16(r, RT_RXBUFTAIL, TAILREG_TO_TAIL(RTL_READ_16(r, RT_RXBUFHEAD)));
		rc = ERR_TOO_BIG;
		release_sem = true;
		goto out;
	}
	if(tail + len > 0xffff) {
//		dprintf("packet wraps around\n");
		memcpy(buf, (const void *)&entry->data[0], 0x10000 - (tail + 4));
		memcpy((uint8 *)buf + 0x10000 - (tail + 4), (const void *)r->rxbuf, len - (0x10000 - (tail + 4)));
	} else {
		memcpy(buf, (const void *)&entry->data[0], len);
	}
	rc = len;

	// calculate the new tail
	tail = ((tail + entry->len + 4 + 3) & ~3) % 0x10000;
//	dprintf("new tail at 0x%x, tailreg will say 0x%x\n", tail, TAIL_TO_TAILREG(tail));
	RTL_WRITE_16(r, RT_RXBUFTAIL, TAIL_TO_TAILREG(tail));

	if(tail != RTL_READ_16(r, RT_RXBUFHEAD)) {
		// we're at last one more packet behind
		release_sem = true;
	}

out:
	release_spinlock(&r->reg_spinlock);
	int_restore_interrupts();

	if(release_sem)
		sem_release(r->rx_sem, 1);
	mutex_unlock(&r->lock);

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

static int rhine_rxint(rhine *r, uint16 int_status)
{
	PANIC_UNIMPLEMENTED();
#if 0
	int rc = INT_NO_RESCHEDULE;

//	dprintf("rx\n");

//	dprintf("buf 0x%x, head 0x%x, tail 0x%x\n",
//		RTL_READ_32(r, RT_RXBUF), RTL_READ_16(r, RT_RXBUFHEAD), RTL_READ_16(r, RT_RXBUFTAIL));
//	dprintf("BUF_EMPTY = %d\n", RTL_READ_8(r, RT_CHIPCMD) & RT_CMD_RX_BUF_EMPTY);

	if(!(RTL_READ_8(r, RT_CHIPCMD) & RT_CMD_RX_BUF_EMPTY)) {
		sem_release_etc(r->rx_sem, 1, SEM_FLAG_NO_RESCHED);
		rc = INT_RESCHEDULE;
	}

	return rc;
#endif
}

static int rhine_txint(rhine *r, uint16 int_status)
{
	PANIC_UNIMPLEMENTED();
#if 0
	uint32 txstat;
	int i;
	int rc = INT_NO_RESCHEDULE;

	// transmit ok
//	dprintf("tx %d\n", int_status);
	if(int_status & RT_INT_TX_ERR) {
		dprintf("err tx int:\n");
		rhine_dumptxstate(r);
	}

	for(i=0; i<4; i++) {
		if(i > 0 && r->last_txbn == r->txbn)
			break;
		txstat = RTL_READ_32(r, RT_TXSTATUS0 + r->last_txbn*4);
//		dprintf("txstat[%d] = 0x%x\n", r->last_txbn, txstat);

		if((txstat & (RT_TX_STATUS_OK | RT_TX_UNDERRUN | RT_TX_ABORTED)) == 0)
			break;

		if(++r->last_txbn >= 4)
			r->last_txbn = 0;
		sem_release_etc(r->tx_sem, 1, SEM_FLAG_NO_RESCHED);
		rc = INT_RESCHEDULE;
	}

	return rc;
#endif
}

static int rhine_int(void* data)
{
	int rc = INT_NO_RESCHEDULE;
	rhine *r = (rhine *)data;
	uint16 istat;
	
	acquire_spinlock(&r->reg_spinlock);

	istat = RHINE_READ_16(r, RHINE_ISR0);
	dprintf("rhine_int: istat 0x%x\n", istat);	
	if (istat == 0)
		goto done;

	if (istat & 0x1) { 
		// packet received with no errors
		dprintf("packet received: status 0x%x, framelen 0x%x\n", 
				RXDESC(r, r->rx_head).status, RXDESC(r, r->rx_head).framelen);
		r->rx_head++;
	}
	
	RHINE_WRITE_16(r, RHINE_ISR0, istat);

#if 0
	// Disable interrupts
	RTL_WRITE_16(r, RT_INTRMASK, 0);

	for(;;) {
		uint16 status = RTL_READ_16(r, RT_INTRSTATUS);
		if(status)
			RTL_WRITE_16(r, RT_INTRSTATUS, status);
		else
			break;

		if(status & RT_INT_TX_OK || status & RT_INT_TX_ERR) {
			if(rhine_txint(r, status) == INT_RESCHEDULE)
				rc = INT_RESCHEDULE;
		}
		if(status & RT_INT_RX_ERR || status & RT_INT_RX_OK) {
			if(rhine_rxint(r, status) == INT_RESCHEDULE)
				rc = INT_RESCHEDULE;
		}
		if(status & RT_INT_RXBUF_OVERFLOW) {
			dprintf("RX buffer overflow!\n");
			dprintf("buf 0x%x, head 0x%x, tail 0x%x\n",
				RTL_READ_32(r, RT_RXBUF), RTL_READ_16(r, RT_RXBUFHEAD), RTL_READ_16(r, RT_RXBUFTAIL));
			RTL_WRITE_32(r, RT_RXMISSED, 0);
			RTL_WRITE_16(r, RT_RXBUFTAIL, TAIL_TO_TAILREG(RTL_READ_16(r, RT_RXBUFHEAD)));
		}
		if(status & RT_INT_RXFIFO_OVERFLOW) {
			dprintf("RX fifo overflow!\n");
		}
		if(status & RT_INT_RXFIFO_UNDERRUN) {
			dprintf("RX fifo underrun\n");
		}
	}

	// reenable interrupts
	RTL_WRITE_16(r, RT_INTRMASK, MYRT_INTS);
#endif

done:
	release_spinlock(&r->reg_spinlock);

	return rc;
}

