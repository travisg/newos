/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/kernel.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/sem.h>
#include <kernel/int.h>
#include <kernel/module.h>

#include <kernel/bus/ide/ide.h>
#include <kernel/bus/isa/isa.h>

#define debug_level_flow 3
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "IDE ISA -- "

#include <kernel/debug_ext.h>


static isa_bus_manager *isa;
static ide_bus_manager_interface *ide;

typedef struct channel_info {
	uint16	command_block_base;
	uint16	control_block_base;

	int		intnum;
	int		interrupt_gate;
	
	sem_id	intsem;
	
	ide_bus_cookie manager_cookie;
} channel_info;

static int channel_count;

extern ide_controller_interface isa_controller_interface;

static ide_controller_params isa_controller_params = {
	dma_alignment : 0,		// requiring even addresses would improve PIO, but's
							// not necessary as the bus manager can handle odd addresses
	dma_boundary : 0,
	dma_boundary_solid : false,
	max_sg_num : 512,		// actually, we don't care; this should be almost infinit
							// but as the block device manager allocates SG buffers 
							// large enough to handle worst case scenarios, we
							// better keep it reasonable small
	max_blocks : 256,
	
	max_devices : 2,
	can_DMA : false,
	can_CQ : false
};

static int write_command_block_regs( channel_info *channel, ide_task_file *tf, 
									 ide_reg_mask mask )
{
	int i;
	uint16 ioaddr = channel->command_block_base;

	for( i = 0; i < 7; i++ ) {
		if( ((1 << (i-7)) & mask) != 0 ) {
			SHOW_FLOW( 3, "%x->HI(%x)", tf->raw.r[i + 7], i );
			isa->write_io_8( ioaddr + 1 + i, tf->raw.r[i + 7] );
		}
		
		if( ((1 << i) & mask) != 0 ) {
			SHOW_FLOW( 3, "%x->LO(%x)", tf->raw.r[i], i );
			isa->write_io_8( ioaddr + 1 + i, tf->raw.r[i] );
		}
	}
	
	return NO_ERROR;
}

static int read_command_block_regs( channel_info *channel, ide_task_file *tf, 
									ide_reg_mask mask )
{
	int i;
	uint16 ioaddr = channel->command_block_base;

	for( i = 0; i < 7; i++ ) {
		if( ((1 << i) & mask) != 0 ) {
			tf->raw.r[i] = isa->read_io_8( ioaddr + 1 + i );
			SHOW_FLOW( 3, "%x: %x", i, (int)tf->raw.r[i] );
		}
	}
	
	return NO_ERROR;
}

static uint8 get_altstatus(channel_info *channel)
{
	uint16 altstatusaddr = channel->control_block_base;

	return isa->read_io_8( altstatusaddr );
}

static int write_device_control( channel_info *channel, uint8 val )
{
	uint16 device_control_addr = channel->control_block_base;
	
	SHOW_FLOW( 3, "%x", (int)val );
	
	isa->write_io_8( device_control_addr, val );
	
	return NO_ERROR;
}

static int write_pio_16( channel_info *channel, uint16 *data, int count, 
	bool force_16bit )
{
	uint16 ioaddr = channel->command_block_base;
	
	force_16bit = true;

	if( (count & 1) != 0 || force_16bit ) {
		for( ; count > 0; --count )
			isa->write_io_16( ioaddr, *(data++) );
			
	} else {
		uint32 *cur_data = (uint32 *)data;
		
		for( ; count > 0; count -= 2 )
			isa->write_io_32( ioaddr, *(cur_data++) );
	}
	
	return NO_ERROR;
}

static int read_pio_16( channel_info *channel, uint16 *data, int count, 
	bool force_16bit )
{
	uint16 ioaddr = channel->command_block_base;
	
	force_16bit = true;

	if( (count & 1) != 0 || force_16bit ) {
		for( ; count > 0; --count )
			*(data++) = isa->read_io_16( ioaddr  );
			
	} else {
		for( ; count > 0; count -= 2 )
			*(data++) = isa->read_io_32( ioaddr );
	}
	
	return NO_ERROR;
}

static int inthand( channel_info *channel )
{
	uint16 altstatusaddr = channel->control_block_base;
	
	SHOW_FLOW0( 3, "" );

	// acknowledge IRQ
	isa->read_io_8( channel->command_block_base + 7 );

	// we expect the device to be non-busy, if it is, we are in trouble
	if( (isa->read_io_8( altstatusaddr ) & ide_status_bsy) != 0 )
		return INT_NO_RESCHEDULE;
		
	SHOW_FLOW0( 3, "1" );

	if( channel->manager_cookie == NULL )
		return INT_NO_RESCHEDULE;
		
	SHOW_FLOW0( 3, "2" );
		
	return ide->irq_handler( channel->manager_cookie );	
}

static int prepare_dma( channel_info *channel, const iovec *vec, size_t count,
	uint32 startbyte, uint32 blocksize,
	size_t *numBytes, bool to_device )
{
	return ERR_NOT_ALLOWED;
}

static int start_dma( void *channel, int device )
{
	return ERR_NOT_ALLOWED;
}

static int finish_dma( void *channel, int device )
{
	return ERR_NOT_ALLOWED;
}


static int probe_channel( uint16 command_block_base, uint16 control_block_base, 
	int intnum, const char *name )
{
	channel_info *new_channel;
	int res;
	
	SHOW_FLOW0( 3, "" );

	new_channel = kmalloc( sizeof(channel_info) );
	if( new_channel == NULL ) {
		res = ERR_NO_MEMORY;
		goto err1;
	}
	
	// we don't care if io-ports are in use already - only 
	// the PCI IDE driver can own them
	if( isa->allocate_ioports( command_block_base, 8 )) {
		res = NO_ERROR;
		goto err2;
	}
	
	if( isa->allocate_ioports( control_block_base, 4 )) {
		res = NO_ERROR;
		goto err3;
	}
	
	new_channel->command_block_base = command_block_base;
	new_channel->control_block_base = control_block_base;
	new_channel->intnum = intnum;

	res = new_channel->intsem = sem_create( 0, "isaide_intsem" );
	if( res < NO_ERROR )
		goto err4;
		
	new_channel->interrupt_gate = 1;
	res = int_set_io_interrupt_handler( new_channel->intnum + 0x20, 
		(int (*)(void *))inthand, new_channel );
	
	if( res ) {
		SHOW_ERROR0( 0, "IDE PCI -- new_isa_device: "
			"could not install interrupt handler");
		goto err5;
	}

	// enable interrupts before connecting channel - detection
	// of devices instantly starts and needs IRQs
	write_device_control(new_channel, ide_devctrl_bit3 );
	
	res = ide->connect_channel( name, &isa_controller_interface, 
		(ide_channel_cookie)new_channel, &isa_controller_params,
		&new_channel->manager_cookie );
		
	if( res )
		goto err6;
	
	++channel_count;
	
	SHOW_FLOW( 3, "registered (total: %i channels)", channel_count );

	return NO_ERROR;

err6:
	write_device_control(new_channel, ide_devctrl_bit3 | ide_devctrl_nien );
err5:
	sem_delete( new_channel->intsem );
err4:
	isa->release_ioports( control_block_base, 4 );
err3:
	isa->release_ioports( command_block_base, 8 );
err2:
	kfree( new_channel );
err1:
	return res;
}

static int release_channel( channel_info *channel )
{
	write_device_control( channel, ide_devctrl_bit3 | ide_devctrl_nien );
	
	// catch spurious interrupt
	thread_snooze( 1000 );
	
	int_remove_io_interrupt_handler( channel->intnum, (int (*)(void *))inthand, channel );
	isa->release_ioports( channel->control_block_base, 4 );
	isa->release_ioports( channel->command_block_base, 8 );
	sem_delete( channel->intsem);

	kfree( channel );

	--channel_count;
	
	return NO_ERROR;
}

static int uninit()
{
	SHOW_FLOW0( 0, "");
	
	if( channel_count != 0 ) {
		SHOW_ERROR( 0, "IDE PCI -- uninit: still %i channels in use", channel_count );
		return ERR_NOT_ALLOWED;
	}
	
	if( isa )
		module_put( ISA_MODULE_NAME );
	if( ide )
		module_put( IDE_BUS_MANAGER_NAME );

	return NO_ERROR;
}

static int find_channels( void )
{
	int res;
	
	SHOW_FLOW0( 3, "" );
	
	res = probe_channel( 0x1f0, 0x3f6, 14, "primary" );
	if( res )
		return res;
		
/*	res = probe_channel( 0x170, 0x376, 15, "secondary" );
	if( res )
		return res;*/
		
	return NO_ERROR;
}


static int init()
{
	int res;
	
	SHOW_FLOW0( 0, "");

	channel_count = 0;
	
	isa = NULL;
	ide = NULL;

	res = module_get( ISA_MODULE_NAME, 0, (void **)&isa );
	if( res )
		goto err;

	res = module_get( IDE_BUS_MANAGER_NAME, 0, (void **)&ide );
	if( res )
		goto err;

	res = find_channels();
	if( res )
		goto err;

	return NO_ERROR;

err:
	uninit();
	return res;
}


static ide_controller_interface isa_controller_interface = {
	(int (*)(ide_channel_cookie,
		ide_task_file*,ide_reg_mask))					&write_command_block_regs,
	(int (*)(ide_channel_cookie,
		ide_task_file*,ide_reg_mask))					&read_command_block_regs,
	 
	(uint8 (*)(ide_channel_cookie))						&get_altstatus,
	(int (*)(ide_channel_cookie,uint8))					&write_device_control,
	
	(int (*)(ide_channel_cookie,uint16*,int,bool))		&write_pio_16,
	(int (*)(ide_channel_cookie,uint16*,int,bool))		&read_pio_16,
	
	(int (*)(ide_channel_cookie,const iovec *,size_t,
		uint32,uint32,size_t*,bool))					&prepare_dma,
	(int (*)(ide_channel_cookie))						&start_dma,
	(int (*)(ide_channel_cookie))						&finish_dma,
	
	(int (*)(ide_channel_cookie))						&release_channel
};

#define IDE_ISA_MODULE_NAME "busses/ide/ide_isa"

// add an empty entry to make module handler happy
static int dummy_interface[] = { 0 };

static module_header ide_isa_module = {
	IDE_ISA_MODULE_NAME,
	MODULE_CURR_VERSION,
	0,
	dummy_interface,
	
	init,
	uninit
};

module_header *modules[] = {
	&ide_isa_module,
	NULL
};
