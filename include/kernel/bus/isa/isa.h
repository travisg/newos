/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

// this is not to be taken seriously - it's just a simple module

#ifndef __ISA_H__
#define __ISA_H__

#define ISA_MODULE_NAME "bus_managers/isa/v1"

typedef struct {
	uint8 (*read_io_8) (int mapped_io_addr);
	void (*write_io_8) (int mapped_io_addr, uint8 value);
	uint16 (*read_io_16) (int mapped_io_addr);
	void (*write_io_16) (int mapped_io_addr, uint16 value);
	uint32 (*read_io_32) (int mapped_io_addr);
	void (*write_io_32) (int mapped_io_addr, uint32 value);

	int (*allocate_iomem) ( addr base, size_t len );
	int (*release_iomem) ( addr base, size_t len );

	int (*allocate_ioports) ( uint16 ioport_base, size_t len );
	int (*release_ioports) ( uint16 ioport_base, size_t len );

	int (*get_dma_buffer) ( void **vaddr, void **paddr );
	int (*start_floppy_dma) ( void *paddr, size_t size, bool write ); // XXX make this more generic
} isa_bus_manager;

#endif
