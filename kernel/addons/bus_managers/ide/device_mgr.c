/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "ide_internal.h"
#include "device_mgr.h"

#include <kernel/bus/scsi/endian.h>
#include <kernel/heap.h>
#include <nulibc/string.h>

#include "queuing.h"
#include "sync.h"
#include "ide_sim.h"
#include "ide_cmds.h"
#include "basic_prot.h"
#include "ata.h"
#include "atapi.h"

bool initialize_buffer( ide_device_info *device, size_t buffer_size )
{
	struct sg_elem *cur_elem;
	uchar *cur_buffer;
	size_t offset;
	
	device->buffer = kmalloc( buffer_size );
	if( device->buffer == NULL )
		return false;
		
	device->buffer_sg_list = kmalloc( 
		(buffer_size + (PAGE_SIZE - 1)) / PAGE_SIZE 
		* sizeof( struct sg_elem ));
	if( device->buffer_sg_list == NULL )
		return false;
	
	device->buffer_size = buffer_size;
	device->buffer_sglist_cnt = 0;

	cur_elem = device->buffer_sg_list;
	cur_buffer = device->buffer;
	
	for( offset = 0; offset < buffer_size; ) {
		int res;
		
		res = vm_get_page_mapping( vm_get_kernel_aspace_id(), (addr)(&cur_buffer),
			(addr *)&cur_elem->cam_sg_address );
			
		if( res != NO_ERROR )
			return false;
			
		cur_elem->cam_sg_count = PAGE_SIZE - 
			((addr)cur_elem->cam_sg_address % PAGE_SIZE);
		cur_buffer += cur_elem->cam_sg_count;
		offset += cur_elem->cam_sg_count;
		++cur_elem;
		++device->buffer_sglist_cnt;
	}
	
	return true;
}

static void destroy_buffer( ide_device_info *device )
{
	if( device->buffer )
		kfree( device->buffer );
		
	if( device->buffer_sg_list )
		kfree( device->buffer_sg_list );
		
	device->buffer = NULL;
	device->buffer_sg_list = NULL;
	device->buffer_size = 0;
}

static void cleanup_device_links( ide_device_info *device )
{
	ide_bus_info *bus = device->bus;
	
	bus->devices[device->is_device1] = NULL;
	
	if( device->other_device ) {
		if( device->other_device != device ) {
			device->other_device->other_device = device->other_device;
			bus->first_device = device->other_device;
		} else
			bus->first_device = NULL;
	}
	
	device->other_device = NULL;
}

static void destroy_device( ide_device_info *device )
{
	SHOW_FLOW0( 3, "" );
	// paranoia
	device->exec_io = NULL;
	timer_cancel_event( &device->reconnect_timer );
	
	cleanup_device_links( device );
	
	destroy_qreq_array( device );
	destroy_buffer( device );
		
	uninit_synced_pc( &device->reconnect_timeout_synced_pc );
	
	kfree( device );
}

static void setup_device_links( ide_bus_info *bus, ide_device_info *device )
{
	SHOW_FLOW0( 3, "" );
	
	device->bus = bus;
	bus->devices[device->is_device1] = device;
		
	device->other_device = device;

	if( device->is_device1 ) {
		if( bus->devices[0] ) {
			device->other_device = bus->devices[0];
			bus->devices[0]->other_device = device;
		}
	} else {
		if( bus->devices[1] ) {
			device->other_device = bus->devices[1];
			bus->devices[1]->other_device = device;
		}
	}
	
	if( bus->first_device == NULL )
		bus->first_device = device;
}

static ide_device_info *create_device( ide_bus_info *bus, bool is_device1 )
{
	ide_device_info *device;
	
	SHOW_FLOW( 3, "is_device1=%i", (int)is_device1 );
	
	device = kmalloc( sizeof( *device ));
	if( device == NULL ) 
		return NULL;
	
	memset( device, 0, sizeof( *device ));

	device->is_device1 = is_device1;
	device->target_id = is_device1;
	
	setup_device_links( bus, device );
	
	device->DMA_failures = 0;
	device->CQ_failures = 0;
	
	device->combined_sense = 0;
	
	device->num_running_reqs = 0;
	
	device->no_reconnect_timeout = true;
	timer_setup_timer( reconnect_timeout, device, &device->reconnect_timer );
	init_synced_pc( &device->reconnect_timeout_synced_pc, 
		reconnect_timeout_worker );
	
	if( xpt->alloc_dpc( &device->reconnect_timeout_dpc ) != NO_ERROR )
		goto err;
		
	device->total_sectors = 0;
	return device;
	
err:
	destroy_device( device );
	return NULL;
}

static void prep_infoblock( ide_device_info *device )
{
	ide_device_infoblock *infoblock = &device->infoblock;

	swap16_multi( (uint16 *)infoblock->serial_number, 
		sizeof( infoblock->serial_number ) / 2 );
		
	swap16_multi( (uint16 *)infoblock->firmware_version, 
		sizeof( infoblock->firmware_version ) / 2 );
		
	swap16_multi( (uint16 *)infoblock->model_number, 
		sizeof( infoblock->model_number ) / 2 );

	infoblock->LBA_total_sectors = letoh32( infoblock->LBA_total_sectors );
	infoblock->LBA48_total_sectors = letoh64( infoblock->LBA48_total_sectors );
}

static bool scan_device_int( ide_device_info *device, bool atapi )
{
	ide_bus_info *bus = device->bus;
	int dummy;
	int status;
	int res;
	
	SHOW_FLOW0( 3, "" );
	
	device->tf_param_mask = 0;
	device->tf.write.command = atapi ? IDE_CMD_IDENTIFY_PACKET_DEVICE
		: IDE_CMD_IDENTIFY_DEVICE;
		
	// initialize device selection flags,
	// this is the only place where this bit gets initialized
	if( bus->controller->read_command_block_regs( bus->channel, &device->tf, 
		ide_mask_device_head ) != NO_ERROR ) 
		return false;
		
	device->tf.lba.device = device->is_device1;

	if( !send_command( device, atapi ? false : true, 20000000, ide_state_sync_waiting ))
		return false;
		
	SHOW_FLOW0( 3, "1" );
	
	// do a short wait first - if there's no device at all we could wait forever
	if( sem_acquire_etc( bus->sync_wait_sem, 1, SEM_FLAG_TIMEOUT, 
		100000, &dummy ) == ERR_SEM_TIMED_OUT ) 
	{
		bool cont;
		
		SHOW_FLOW0( 3, "no fast response to inquiry" );
		
		// check the busy flag - if it's still set, there's probably no device	
		IDE_LOCK( bus );
	
		status = bus->controller->get_altstatus( bus->channel );
		SHOW_FLOW( 3, "status=%x", (int)status );
		cont = (status & ide_status_bsy) == ide_status_bsy;
		
		IDE_UNLOCK( bus );
		
		if( !cont ) {
			SHOW_FLOW0( 3, "no busy bit not set after 100ms - probably noone there" );
			// no reaction -> abort waiting
			cancel_irq_timeout( bus );
			
			// timeout or irq may have been fired, reset semaphore just is case
			sem_acquire_etc( bus->sync_wait_sem, 1, SEM_FLAG_TIMEOUT, 0, &dummy );
			
			return false;
		}
		
		SHOW_FLOW0( 3, "busy bit set, give device more time" );
		// there is something, so wait for it
		sem_acquire( bus->sync_wait_sem, 1 );
	}

	if( bus->sync_wait_timeout )
		return false;	

	SHOW_FLOW0( 3, "2" );
	
	ide_wait( device, ide_status_drq, ide_status_bsy, true, 1000 );
	
	status = bus->controller->get_altstatus( bus->channel );
	
	if( (status & ide_status_err) != 0 ) {
		// if there's no device, all bits including the error bit are set
		SHOW_FLOW0( 3, "err set" );
		return false;
	}
	
	SHOW_FLOW0( 3, "3" );
		
	bus->controller->read_pio_16( bus->channel, (uint16 *)&device->infoblock, 
		sizeof( device->infoblock ) / sizeof( uint16 ), false );
		
	SHOW_FLOW0( 3, "4" );
	
	if( !wait_for_drqdown( device ))
		return false;
		
	SHOW_FLOW0( 3, "5" );
		
	prep_infoblock( device );
	return true;	
}

void scan_device_worker( ide_bus_info *bus, void *arg )
{
	int is_device1 = (int)arg;
	ide_device_info *device;
	
	SHOW_FLOW0( 3, "" );
	
	if( bus->devices[is_device1] )
		destroy_device( bus->devices[is_device1] );
		
	device = create_device( bus, is_device1 );
	
	if( scan_device_int( device, false )) {
		if( !prep_ata( device ))
			goto err;
	} else if( scan_device_int( device, true )) {
		if( !prep_atapi( device ))
			goto err;
	} else
		goto err;

	bus->state = ide_state_idle;
	sem_release( bus->scan_device_sem, 1 );
	return;
	
err:
	destroy_device( device );
	
	bus->state = ide_state_idle;
	sem_release( bus->scan_device_sem, 1 );
	return;
}
