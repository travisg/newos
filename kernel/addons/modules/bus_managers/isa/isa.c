/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


// TODO:
// - forward iomem/ioport allocation to central resource manager

#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/module.h>
#include <kernel/bus/isa/isa.h>
#include <kernel/arch/cpu.h>

#define debug_level_flow 3
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "ISA -- "

#include <kernel/debug.h>
#include <kernel/debug_ext.h>

struct isa_info {
	region_id dma_region_id;
	void *dma_region_base;
} isa_info;

static int isa_init( void )
{
	vm_region_info rinfo;

	SHOW_FLOW0( 3, "" );

	// lookup the dma region
	isa_info.dma_region_id = vm_find_region_by_name(vm_get_kernel_aspace_id(), "dma_region");
	if(isa_info.dma_region_id < 0) {
		return ERR_GENERAL;
	}

	// find it's base
	vm_get_region_info(isa_info.dma_region_id, &rinfo);
	isa_info.dma_region_base = (void *)rinfo.base;

	return NO_ERROR;
}

static int isa_uninit( void )
{
	SHOW_FLOW0( 3, "" );
	return NO_ERROR;
}

static uint8 isa_read_io_8( int mapped_io_addr )
{
	uint8 value;

	value = in8( mapped_io_addr );

	SHOW_FLOW( 3, "%x->%x", mapped_io_addr, (int)value );

	return value;
}

static void isa_write_io_8( int mapped_io_addr, uint8 value )
{
	SHOW_FLOW( 3, "%x->%x", (int)value, mapped_io_addr );

	out8( value, mapped_io_addr );
}

static uint16 isa_read_io_16( int mapped_io_addr )
{
	return in16( mapped_io_addr );
}

static void isa_write_io_16( int mapped_io_addr, uint16 value )
{
	out16( value, mapped_io_addr );
}

static uint32 isa_read_io_32( int mapped_io_addr )
{
	return in32( mapped_io_addr );
}

static void isa_write_io_32( int mapped_io_addr, uint32 value )
{
	out32( value, mapped_io_addr );
}


static int isa_allocate_iomem( addr_t base, size_t len )
{
	return ERR_UNIMPLEMENTED;
}

static int isa_release_iomem( addr_t base, size_t len )
{
	return ERR_UNIMPLEMENTED;
}

static int isa_allocate_ioports( uint16 ioport_base, size_t len )
{
	return ERR_UNIMPLEMENTED;
}

static int isa_release_ioports( uint16 ioport_base, size_t len )
{
	return ERR_UNIMPLEMENTED;
}

static int isa_get_dma_buffer( void **vaddr, void **paddr )
{
	// for now, return the second block of the dma region
	*vaddr = (void *)((addr_t)isa_info.dma_region_base + 64*1024);
	*paddr = (void *)(64*1024);

	return NO_ERROR;
}

static int isa_start_floppy_dma( void *paddr, size_t size, bool write )
{
	enum {
		kDMAAddress = 4,
		kDMACount = 5,
		kDMACommand = 8,
		kDMAChannelMask = 0xa,
		kDMAMode = 0xb,
		kDMAFlipFlop = 0xc,
		kDMAPage = 0x81,
	};

	// Set up the DMA controller to transfer data from the floppy
	// controller.
	out8(0x14, kDMACommand);	// Disable DMA
	if(write)
		out8(0x5a, kDMAMode); // single mode, autoinitialization, read transfer, channel 2
	else
		out8(0x56, kDMAMode); // single mode, autoinitialization, write transfer, channel 2
	out8(0, kDMAFlipFlop);
	out8(((addr_t)paddr) & 0xff, kDMAAddress);			// LSB
	out8((((addr_t)paddr) >> 8) & 0xff, kDMAAddress);	// MSB
	out8((((addr_t)paddr) >> 16) & 0xff, kDMAPage);		// Page
	out8(0, kDMAFlipFlop);
	size--; // the register wants it to be one less than it really is
	out8(size & 0xff, kDMACount);	// Count LSB
	out8((size >> 8) & 0xff, kDMACount); // Count MSB
	out8(2, kDMAChannelMask);	// Release channel 2
	out8(0x10, kDMACommand);	// Reenable DMA

	return 0;
}

isa_bus_manager isa_interface = {
	isa_read_io_8, isa_write_io_8,
	isa_read_io_16, isa_write_io_16,
	isa_read_io_32, isa_write_io_32,

	isa_allocate_iomem,
	isa_release_iomem,
	isa_allocate_ioports,
	isa_release_ioports,

	isa_get_dma_buffer,
	isa_start_floppy_dma,
};


static module_header isa_module = {
	ISA_MODULE_NAME,
	MODULE_CURR_VERSION,
	MODULE_KEEP_LOADED,

	&isa_interface,

	&isa_init,
	&isa_uninit
};

module_header *modules[] = {
	&isa_module,
	NULL
};
