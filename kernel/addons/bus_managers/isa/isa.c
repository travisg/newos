/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/module.h>
#include <kernel/bus/isa/isa.h>
#include <kernel/arch/cpu.h>
#include <kernel/debug.h>


static int isa_init( void )
{
	dprintf( "ISA: init\n" );
	return NO_ERROR;
}


static int isa_uninit( void )
{
	dprintf( "ISA: uninit\n" );
	return NO_ERROR;
}

static uint8 isa_read_io_8( int mapped_io_addr )
{
	return in8( mapped_io_addr );
}

static void isa_write_io_8( int mapped_io_addr, uint8 value )
{
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

isa_bus_manager isa_interface = {
	isa_read_io_8, isa_write_io_8,
	isa_read_io_16, isa_write_io_16,
	isa_read_io_32, isa_write_io_32
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
