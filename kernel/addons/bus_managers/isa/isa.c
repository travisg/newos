/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


// TODO:
// - forward iomem/ioport allocation to central resource manager

#include <kernel/module.h>
#include <kernel/bus/isa/isa.h>
#include <kernel/arch/cpu.h>

#define debug_level_flow 3
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "ISA -- "

#include <kernel/debug.h>
#include <kernel/debug_ext.h>


static int isa_init( void )
{
	SHOW_FLOW0( 3, "" );
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


static int isa_allocate_iomem( addr base, size_t len )
{
	return NO_ERROR;
}

static int isa_release_iomem( addr base, size_t len )
{
	return NO_ERROR;
}
	
static int isa_allocate_ioports( uint16 ioport_base, size_t len )
{
	return NO_ERROR;
}

static int isa_release_ioports( uint16 ioport_base, size_t len )
{
	return NO_ERROR;
}


isa_bus_manager isa_interface = {
	isa_read_io_8, isa_write_io_8,
	isa_read_io_16, isa_write_io_16,
	isa_read_io_32, isa_write_io_32,
	
	isa_allocate_iomem,
	isa_release_iomem,
	isa_allocate_ioports,
	isa_release_ioports,
};


static module_header isa_module = {
	ISA_MODULE_NAME,
	MODULE_CURR_VERSION,
	0,
	
	&isa_interface,
	
	isa_init,
	isa_uninit
};

module_header *modules[] = {
	&isa_module,
	NULL
};
