/*
** Copyright 2001, Graham Batty. All rights reserved.
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

#include <kernel/bus/pci/pci.h>
#include <string.h>

#include "pcnet32_dev.h"
#include "pcnet32_priv.h"

//#define _PCNET32_VERBOSE

#ifdef _PCNET32_VERBOSE
# define debug_level_flow 3
# define debug_level_error 3
# define debug_level_info 3

# define DEBUG_MSG_PREFIX "PCNET_DEV -- "

# include <kernel/debug_ext.h>
#else
# define SHOW_FLOW(a,b,...)
# define SHOW_FLOW0(a,b)
#endif

// mapping of initialization block rx and txlengths
static uint16 gringlens[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 };


#define WRITE_8(nic, reg, dat) \
	out8(dat, (nic)->io_port + (reg))
#define WRITE_16(nic, reg, dat) \
	out16(dat, (nic)->io_port + (reg))
#define WRITE_32(nic, reg, dat) \
	out32(dat, (nic)->io_port + (reg))

#define READ_8(nic, reg) \
	in8((nic)->io_port + (reg))
#define READ_16(nic, reg) \
	in16((nic)->io_port + (reg))
#define READ_32(nic, reg) \
	in32((nic)->io_port + (reg))

#define RXRING_INDEX(_nic, _index) ((_index) & (_nic->rxring_count - 1))
#define TXRING_INDEX(_nic, _index) ((_index) & (_nic->txring_count - 1))

#define RXRING_BUFFER(_nic, _index) ((_nic)->rx_buffers + ((_index) * (_nic)->rx_buffersize))
#define TXRING_BUFFER(_nic, _index) ((_nic)->tx_buffers + ((_index) * (_nic)->tx_buffersize))
#define BUFFER_PHYS(_nic, _buffer) (addr)((_nic)->buffers_phys + ((_buffer) - (_nic)->buffers))

static int pcnet32_int(void*);

// call this to enable a receive buffer so the controller can fill it.
static void rxdesc_init(pcnet32 *nic, uint16 index);
static int pcnet32_thread(void *nic);

static uint16 read_csr(pcnet32 *nic, uint32 reg)
{
	WRITE_32(nic, PCNET_IO_ADDRPORT, reg);
	return READ_32(nic, PCNET_IO_DATAPORT);
}

static uint16 read_bcr(pcnet32 *nic, uint32 reg)
{
	WRITE_32(nic, PCNET_IO_ADDRPORT, reg);
	return READ_32(nic, PCNET_IO_CONFIGPORT);
}

static void write_csr(pcnet32 *nic, uint32 reg, uint16 data)
{
	WRITE_32(nic, PCNET_IO_ADDRPORT, reg);
	WRITE_32(nic, PCNET_IO_DATAPORT, data);
}

static void modify_csr(pcnet32 *nic, uint32 reg, uint16 bits, uint16 set)
{
	uint16 oldset;

	acquire_spinlock(&nic->control_lock);
	int_disable_interrupts();

	// do stuff
	oldset = read_csr(nic, reg);
	oldset |= (bits & set);
	oldset &= ~(bits & ~set);

	write_csr(nic, reg, oldset);

	int_restore_interrupts();
	release_spinlock(&nic->control_lock);
}

static void write_bcr(pcnet32 *nic, uint32 reg, uint16 data)
{
	WRITE_32(nic, PCNET_IO_ADDRPORT, reg);
	WRITE_32(nic, PCNET_IO_CONFIGPORT, data);
}

pcnet32 *pcnet32_new(uint32 initmode, uint16 rxbuffer_size,	uint16 txbuffer_size)
{
	pcnet32 *nic = NULL;

	uint16 rxring_count = gringlens[(initmode & PCNET_INIT_RXLEN_MASK) >> PCNET_INIT_RXLEN_POS];
	uint16 txring_count = gringlens[(initmode & PCNET_INIT_TXLEN_MASK) >> PCNET_INIT_TXLEN_POS];

	SHOW_FLOW(3, "initmode: 0x%.8x, rxring: (%d, %d), txring: (%d, %d)", 
		  initmode, 
		  rxring_count, rxbuffer_size, 
		  txring_count, txbuffer_size);

	nic = (pcnet32*)kmalloc(sizeof(pcnet32));
	if (nic == NULL)
		return NULL;

	memset(nic, 0, sizeof(pcnet32));

	nic->init_mode = initmode;

	// Make it a 256-descriptor ring. 256*32 bytes long.
	nic->rxring_count = rxring_count;
	nic->rx_buffersize = rxbuffer_size;
	nic->rxring_tail =
	nic->rxring_head = 0;

	nic->rxring_region = vm_create_anonymous_region(
		vm_get_kernel_aspace_id(), "pcnet32_rxring", (void**)&nic->rxring,
		REGION_ADDR_ANY_ADDRESS, nic->rxring_count * sizeof(struct pcnet32_rxdesc), 
		REGION_WIRING_WIRED_CONTIG, LOCK_KERNEL | LOCK_RW);

	if (nic->rxring_region < 0)
		goto err;

	memset(nic->rxring, 0, nic->rxring_count * sizeof(struct pcnet32_rxdesc));
	vm_get_page_mapping(vm_get_kernel_aspace_id(), 
			    (addr)nic->rxring, &nic->rxring_phys);
	SHOW_FLOW(3, "rxring physical address: 0x%x", nic->rxring_phys);

	nic->rxring_sem = sem_create(0, "pcnet32_rxring");
	mutex_init(&nic->rxring_mutex, "pcnet32_rxring");
       
	// setup_transmit_descriptor_ring;
	nic->txring_count = txring_count;
	nic->tx_buffersize = txbuffer_size;
	nic->txring_tail = 
	nic->txring_head = 0;

	nic->txring_region = vm_create_anonymous_region(
		vm_get_kernel_aspace_id(), "pcnet32_txring", (void**)&nic->txring,
		REGION_ADDR_ANY_ADDRESS, nic->txring_count * sizeof(struct pcnet32_txdesc), 
		REGION_WIRING_WIRED_CONTIG, LOCK_KERNEL | LOCK_RW);
	memset(nic->txring, 0, nic->txring_count * sizeof(struct pcnet32_txdesc));
	vm_get_page_mapping(vm_get_kernel_aspace_id(), 
			    (addr)nic->txring, &nic->txring_phys);
	SHOW_FLOW(3, "txring physical address: 0x%x", nic->txring_phys);

	if (nic->txring_region < 0)
		goto err;

	mutex_init(&nic->txring_mutex, "pcnet32_txring");

	// allocate the actual buffers
	nic->buffers_region = vm_create_anonymous_region(
		vm_get_kernel_aspace_id(), "pcnet32_buffers", (void**)&nic->buffers,
		REGION_ADDR_ANY_ADDRESS, 
		(nic->rxring_count * nic->rx_buffersize) +
		(nic->txring_count * nic->tx_buffersize),
		REGION_WIRING_WIRED_CONTIG, LOCK_KERNEL | LOCK_RW);

	vm_get_page_mapping(vm_get_kernel_aspace_id(), 
			    (addr)nic->buffers, &nic->buffers_phys);

	if (nic->buffers_region < 0)
		goto err;

	nic->rx_buffers = (uint8*)nic->buffers;
	nic->tx_buffers = (uint8*)(nic->buffers + nic->rxring_count * nic->rx_buffersize);

	// create the thread
	nic->interrupt_sem = sem_create(0, "pcnet32_interrupt");
	nic->thread = thread_create_kernel_thread("pcnet32_isr", pcnet32_thread, nic);
	thread_set_priority(nic->thread, THREAD_HIGHEST_PRIORITY);

	return nic;

err:
	pcnet32_delete(nic);
	return NULL;
}

int pcnet32_detect(pcnet32 *dev)
{
	int i;
	pci_module_hooks *pci;
	pci_info pinfo;
	bool foundit = false;

	if (module_get(PCI_BUS_MODULE_NAME, 0, (void**)&pci))
	{
		SHOW_FLOW(3, "Could not find PCI Bus.");
		return -1;
	}

	for (i = 0; pci->get_nth_pci_info(i, &pinfo) >= NO_ERROR; i++)
	{
		if (pinfo.vendor_id == AMD_VENDORID && 
		    (pinfo.device_id == PCNET_DEVICEID ||
		     pinfo.device_id == PCHOME_DEVICEID))
		{
			foundit = true;
			break;
		}
	}

	if (!foundit)
	{
		SHOW_FLOW(3, "Could not find PCNET32 Compatible Device");
		return -1;
	}

	dev->irq = pinfo.u.h0.interrupt_line;
	for (i = 0; i < 6; i++)
	{
		if (pinfo.u.h0.base_registers[i] > 0xffff)
		{
			dev->phys_base = pinfo.u.h0.base_registers[i];
			dev->phys_size = pinfo.u.h0.base_register_sizes[i];
		} else if (pinfo.u.h0.base_registers[i] > 0) {
			dev->io_port = pinfo.u.h0.base_registers[i];
		}
	}

	return 0;
}

int pcnet32_init(pcnet32 *nic)
{
	int i;

	if(nic->phys_base == 0) {
		SHOW_FLOW0(3, "nic->phys_base was null.");
		return -1;
	}

// XX looks like there's no way to set busmaster at this point.
//    but pcnet32 needs to be, so this needs to be fixed again.
//	i = sys_open(dev->dev_path, STREAM_TYPE_ANY, 0);
//	sys_ioctl(i, PCI_SET_BUSMASTER, NULL, 0);
//	sys_close(i);

	SHOW_FLOW(3, "detected device at irq %d, memory base 0x%lx, size 0x%lx", 
		  nic->irq, nic->phys_base, nic->phys_size);

	nic->io_region = vm_map_physical_memory(vm_get_kernel_aspace_id(), "pcnet32_region", 
		(void **)&nic->virt_base, REGION_ADDR_ANY_ADDRESS, nic->phys_size, 
		LOCK_KERNEL|LOCK_RW, nic->phys_base);

	if(nic->io_region < 0) {
		SHOW_FLOW(3, "error allocating device physical region at %p", 
			  nic->phys_base);
		return nic->io_region;
	}
	SHOW_FLOW(3, "device mapped at virtual address 0x%lx", nic->virt_base);

	// before we do this we set up our interrupt handler,
	// we want to make sure the device is in a semi-known state, so we
	// do a little control register stuff here.
	write_csr(nic, PCNET_CSR_STATUS, PCNET_STATUS_STOP);

	// set up the interrupt handler
	int_set_io_interrupt_handler(nic->irq + 0x20, &pcnet32_int, nic);

	SHOW_FLOW(3, "device mapped irq 0x%x", nic->irq + 0x20);

	// fetch ethernet address
	for (i = 0; i < 6; i++)
	{
		nic->mac_addr[i] = READ_8(nic, i);
	}
	SHOW_FLOW(3, "MAC Address: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
		  nic->mac_addr[0],
		  nic->mac_addr[1],
		  nic->mac_addr[2],
		  nic->mac_addr[3],
		  nic->mac_addr[4],
		  nic->mac_addr[5]);
      
	return 0;
}

void pcnet32_start(pcnet32 *nic)
{
	bigtime_t time;
	int err = -1;
	addr temp;
	int i = 0;
	struct pcnet32_init *init;

	SHOW_FLOW(3, "starting %p", nic);

	SHOW_FLOW0(3, "resetting device");
	READ_32(nic, PCNET_IO_RESET);
	WRITE_32(nic, PCNET_IO_RESET, 0);

	// give the device time to figure it out
	thread_snooze(10000);

	// set us up in 32-bit wide structure mode
	WRITE_32(nic, PCNET_IO_DATAPORT, 0);
	write_bcr(nic, PCNET_BCR_SWMODE, 0x0002);

	// now we build the initialization block in the
	// buffer area (which is unused right now)
	init = (struct pcnet32_init*)nic->buffers;
	memset(init, 0, sizeof(struct pcnet32_init));

	init->flags = nic->init_mode;
	memcpy(init->paddr, &nic->mac_addr, sizeof(nic->mac_addr));
	init->rxaddr = nic->rxring_phys;
	init->txaddr = nic->txring_phys;

	write_csr(nic, PCNET_CSR_IADDR0, nic->buffers_phys & 0xffff);
	write_csr(nic, PCNET_CSR_IADDR1, (nic->buffers_phys >> 16) & 0xffff);
	write_csr(nic, PCNET_CSR_STATUS, PCNET_STATUS_INIT);

	// wait for it to finish initializing.
	while ( !(read_csr(nic, PCNET_CSR_STATUS) & PCNET_STATUS_IDON) );

	// Initialize the rxring
	for (i = 0; i < nic->rxring_count; i++)
		rxdesc_init(nic, i);

	// Initialize the txring
	memset(nic->txring, 0, nic->txring_count * sizeof(struct pcnet32_txdesc));

	// write the start and interrupt enable bits.
	write_csr(nic, PCNET_CSR_STATUS, PCNET_STATUS_STRT | PCNET_STATUS_IENA);

	// for the sake of verification, we'll read csr0 and dprint it.
	SHOW_FLOW(3, "csr0 (status) = 0x%.4x", read_csr(nic, PCNET_CSR_STATUS));

	// start the thread
	thread_resume_thread(nic->thread);
}

void pcnet32_stop(pcnet32 *nic)
{
	SHOW_FLOW(3, "Stopping pcnet device %p", nic);

	// start the thread
	thread_suspend_thread(nic->thread);

	// stop the device.
	write_csr(nic, PCNET_CSR_STATUS, PCNET_STATUS_STOP);
}

void pcnet32_delete(pcnet32 *nic)
{
	if (nic->txring_region > -1)
		vm_delete_region(vm_get_kernel_aspace_id(), nic->txring_region);

       	mutex_destroy(&nic->txring_mutex);

	if (nic->rxring_region > -1)
		vm_delete_region(vm_get_kernel_aspace_id(), nic->rxring_region);

	if (nic->rxring_sem > -1)
		sem_delete(nic->rxring_sem);

       	mutex_destroy(&nic->rxring_mutex);

	if (nic->io_region > -1)
		vm_delete_region(vm_get_kernel_aspace_id(), nic->io_region);
}

static void rxdesc_init(pcnet32 *nic, uint16 index)
{
	uint16 masked_index = RXRING_INDEX(nic, index);

	struct pcnet32_rxdesc *desc = nic->rxring + masked_index;
	uint32 buffer = BUFFER_PHYS(nic, RXRING_BUFFER(nic, masked_index));
	
	desc->buffer_addr = buffer;
	desc->buffer_length = -nic->rx_buffersize;
	desc->message_length = 0;
	desc->user = 0;
	
	// enable the controller to write to this receive buffer.
	desc->status = PCNET_RXSTATUS_OWN;
}

ssize_t pcnet32_xmit(pcnet32 *nic, const char *ptr, ssize_t len)
{
	uint16 index = 0;
	uint8 *buffer = NULL;

	SHOW_FLOW(3, "nic %p data %p len %d", nic, ptr, len);

	mutex_lock(&nic->txring_mutex);
	index = TXRING_INDEX(nic, nic->txring_head);

	SHOW_FLOW(3, "using txring index %d", index);

	// XXX need support for spanning packets in case a size other than 2048 is used.
        // Examine OWN bit of tx descriptor at tx_queue_tail;
	// if (OWN == 1) return "Out of buffer";
	if ((len > nic->tx_buffersize) ||
	    (nic->txring[index].status & PCNET_TXSTATUS_OWN))
	{
		SHOW_FLOW0(3, "packet was too large or no more txbuffers.");
		return ERR_VFS_INSUFFICIENT_BUF;
	}

	// Get buffer address from descriptor at end_tx_queue;
	buffer = TXRING_BUFFER(nic, index);
	nic->txring[index].buffer_addr = BUFFER_PHYS(nic, buffer);

	// Copy packet to tx buffer;
	memcpy(buffer, ptr, len);

	// Set up BCNT field of descriptor;
	nic->txring[index].buffer_length = -len;

	// Set OWN, STP, and ENP bits of descriptor;
	nic->txring[index].status |= PCNET_TXSTATUS_OWN | 
		PCNET_TXSTATUS_STP | PCNET_TXSTATUS_ENP;

	// tx_queue_tail = tx_queue_tail + 1;
	// if (tx_queue_tail > last_tx_descriptor) tx_queue_tail = first_tx_descriptor;
	nic->txring_head++;

	mutex_unlock(&nic->txring_mutex);

	// return "OK";
	return len;
}

ssize_t pcnet32_rx(pcnet32 *nic, char *buf, ssize_t buf_len)
{
	uint16 i = 0, index = 0;
	ssize_t real_len = -1;

	SHOW_FLOW(3, "nic %p data %p buf_len %d", nic, buf, buf_len);

	// here we loop through consuming rxring descriptors until we get
	// one suitable for returning.
	while (true)
	{
		// consume 1 descriptor at a time, whether it's an error or a
		// valid packet.
		sem_acquire(nic->rxring_sem, 1);

		// the semaphor has been released at least once, so we will
		// lock the rxring and grab the next available descriptor.
		mutex_lock(&nic->rxring_mutex);

		// grab the index we want.
		index = RXRING_INDEX(nic, nic->rxring_tail);

		if (nic->rxring[index].status & PCNET_RXSTATUS_ERR)
		{
			SHOW_FLOW(3, "rxring descriptor %d reported an error: 0x%.4x", 
				  index, nic->rxring[index].status);
		}

		if (nic->rxring[index].status & PCNET_RXSTATUS_OWN)
		{
			SHOW_FLOW(3, "warning: descriptor %d should have been owned by the software is owned by the hardware.", index);

			nic->rxring_tail++;
			mutex_unlock(&nic->rxring_mutex);
			continue; // skip this descriptor altogether.
		} 
		else if ((size_t)buf_len >= nic->rxring[index].message_length) // got one
		{
			real_len = nic->rxring[index].message_length;

			// copy the buffer
			memcpy(buf, RXRING_BUFFER(nic, index), real_len);

			// reinitialize the buffer and give it back to the controller.
			rxdesc_init(nic, index);
			
			// move the tail up.
			nic->rxring_tail++;
			mutex_unlock(&nic->rxring_mutex);
			return real_len;
		}
	}

	return real_len;
}

// these two return false when there is nothing left to do.
static bool pcnet32_rxint(pcnet32 *nic)
{
	uint32 index;

	index = RXRING_INDEX(nic, nic->rxring_head);
	if ( !(nic->rxring[index].status & PCNET_RXSTATUS_OWN) )
	{
		SHOW_FLOW(3, "Got packet len %d, index %d", nic->rxring[index].message_length, index);
		nic->rxring_head++;
		sem_release(nic->rxring_sem, 1);
		return true;
	}

	return false;
}

static bool pcnet32_txint(pcnet32 *nic)
{
	uint32 index;

	index = TXRING_INDEX(nic, nic->txring_tail);
	if ( (nic->txring[index].status & PCNET_TXSTATUS_OWN) )
	{
		SHOW_FLOW(3, "Sent packet %d", index);
		nic->txring_tail++;
		return true;
	} else
        	return false;
}

static int pcnet32_thread(void *data)
{
	pcnet32 *nic = (pcnet32 *)data;

	while (true)
	{
		sem_acquire(nic->interrupt_sem, 1);

		// check if there is a 'fatal error' (BABL and MERR)
		if ((nic->interrupt_status & PCNET_STATUS_ERR) &&
		    (nic->interrupt_status & PCNET_STATUS_BABL ||
		     nic->interrupt_status & PCNET_STATUS_MERR))
		{
			// fatal error. Stop everything and reinitialize the device.
			pcnet32_stop(nic);
			pcnet32_start(nic);
		}
	
		mutex_lock(&nic->rxring_mutex);
		while (pcnet32_rxint(nic));
		mutex_unlock(&nic->rxring_mutex);
		
		mutex_lock(&nic->txring_mutex);
		while (pcnet32_txint(nic));
		mutex_unlock(&nic->txring_mutex);

		// re-enable pcnet interrupts
		write_csr(nic, PCNET_CSR_STATUS, PCNET_STATUS_IENA);
	}
	return 0;
}

static int pcnet32_int(void* data)
{
	pcnet32 *nic = (pcnet32 *)data;
	uint32 status;

	acquire_spinlock(&nic->control_lock);
	int_disable_interrupts();

	status = read_csr(nic, PCNET_CSR_STATUS);

	SHOW_FLOW(3, "handling irq: status 0x%x", status);

	// clear the bits that caused this to happen and disable pcnet interrupts
	// for the time being
	write_csr(nic, PCNET_CSR_STATUS, status & ~PCNET_STATUS_IENA);
	
	int_restore_interrupts();
        release_spinlock(&nic->control_lock);
	
	nic->interrupt_status = status;
	sem_release_etc(nic->interrupt_sem, 1, SEM_FLAG_NO_RESCHED);

	SHOW_FLOW(3, "interrupt handled. pcnet status 0x%.8x", read_csr(nic, PCNET_CSR_STATUS));

	return INT_RESCHEDULE;
}

