/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include "ide_internal.h"
#include "queuing.h"

#include <kernel/heap.h>
#include <string.h>

#include "basic_prot.h"
#include "ide_sim.h"
#include "ata.h"
#include "DMA.h"
#include "sync.h"
#include "ide_cmds.h"


#define MAX_CQ_FAILURES 3

static inline ide_qrequest *tag2request( ide_device_info *device, int tag )
{
	ide_qrequest *qrequest = &device->qreq_array[tag];
	
	if( qrequest->running )
		return qrequest;
	else
		return NULL;
}


// expects spin-locked bus
// returns true if servicing a command (implies having bus unspin-locked)
// returns false and setting idle_state if nothing to service
bool try_service( ide_device_info *device )
{
	ide_bus_info *bus = device->bus;
	ide_device_info *other_device;
	int tag;
	ide_qrequest *qrequest;
	
	other_device = device->other_device;
	
retry:
	if( other_device != device && check_service_req( other_device ))
		device = other_device;
	else if( !check_service_req( device ))
		return false;
	
	IDE_UNLOCK( bus );
	
	timer_cancel_event( &device->reconnect_timer );
		
	if( !device_start_service( device, &tag )) {
		send_abort_queue( device );
		return true;
	}
	
	qrequest = tag2request( device, tag );
	if( qrequest == NULL ) {
		finish_reset_queue( qrequest );
		
		IDE_LOCK( bus );
		goto retry;
	}

	if( !check_rw_error( device, qrequest )) {
		finish_reset_queue( qrequest );
		
		IDE_LOCK( bus );
		goto retry;
	}
	
	bus->active_qrequest = qrequest;

	start_dma( device, qrequest );

	return true;
}

bool initialize_qreq_array( ide_device_info *device, int queue_depth )
{
	int i;
	
	device->queue_depth = queue_depth;
	
	device->qreq_array = kmalloc( queue_depth * sizeof( ide_qrequest ));
	if( device->qreq_array == NULL )
		return false;
		
	memset( device->qreq_array, 0, queue_depth * sizeof( ide_qrequest ));
	
	device->free_qrequests = NULL;
		
	for( i = 0; i < queue_depth; ++i ) {
		ide_qrequest *qrequest = &device->qreq_array[i];
		
		qrequest->next = device->free_qrequests;
		device->free_qrequests = qrequest;
		
		qrequest->running = false;
		qrequest->device = device;
		qrequest->tag = i;
		qrequest->request = NULL;
	}
	
	return true;
}

void destroy_qreq_array( ide_device_info *device )
{
	if( device->qreq_array ) {
		kfree( device->qreq_array );
		device->qreq_array = NULL;
	}
	
	device->num_running_reqs = 0;
	device->queue_depth = 0;
	device->free_qrequests = NULL;
}

static bool change_qreq_array( ide_device_info *device, int queue_depth )
{
	ide_qrequest *qreq_array = device->qreq_array;
	ide_qrequest *old_free_qrequests = device->free_qrequests;
	int old_queue_depth = queue_depth;
	
	if( initialize_qreq_array( device, queue_depth )) {
		kfree( qreq_array );
		return true;

	} else {
		device->qreq_array = qreq_array;
		device->num_running_reqs = 0;
		device->queue_depth = old_queue_depth;
		device->free_qrequests = old_free_qrequests;
		return false;
	}
}

void reset_timeouts( ide_device_info *device )
{
	if( !device->no_reconnect_timeout ) {
		device->no_reconnect_timeout = true;
		timer_cancel_event( &device->reconnect_timer );
	}
}

void reconnect_timeout_worker( ide_bus_info *bus, void *arg )
{
	ide_device_info *device = (ide_device_info *)arg;
	
	send_abort_queue( device );
	
	if( ++device->CQ_failures > MAX_CQ_FAILURES ) {
		device->CQ_enabled = false;
		
		change_qreq_array( device, 1 );
	}
	
	//xpt->unblock_bus( device->bus->xpt_cookie );
}

static void reconnect_timeout_dpc( void *arg )
{
	ide_device_info *device = (ide_device_info *)arg;
	
	//xpt->block_bus( bus->xpt_cookie );

	schedule_synced_pc( device->bus, &device->reconnect_timeout_synced_pc, device );
}

int reconnect_timeout( void * arg )
{
	ide_device_info *device = (ide_device_info *)arg;
	ide_bus_info *bus = device->bus;

	xpt->schedule_dpc( bus->xpt_cookie, device->reconnect_timeout_dpc, 
		reconnect_timeout_dpc, device );
		
	return INT_RESCHEDULE;
}

bool send_abort_queue( ide_device_info *device )
{
	int status;
	ide_bus_info *bus = device->bus;
	
	device->tf.write.command = IDE_CMD_NOP;
	device->tf.write.features = IDE_CMD_NOP_NOP;	// discard outstanding commands
	
	device->tf_param_mask = ide_mask_features;
	
	if( !send_command( device, true, 0, ide_state_accessing ))
		goto err;
		
	if( !wait_for_drdy( device ))
		goto err;

	// device must answer "command rejected" and discard outstanding commands
	status = bus->controller->get_altstatus( bus->channel );	
	
	if( (status & ide_status_err) == 0 ) 
		goto err;
		
	if( !bus->controller->read_command_block_regs( 
		bus->channel, &device->tf, ide_mask_error ))
	{
		// don't bother trying bus_reset as controller disappeared
		set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
		return false;
	}
		
	if( (device->tf.read.error & ide_error_abrt) == 0 )
		goto err;

	finish_all_requests( device, CAM_RESUBMIT );
	return true;
	
err:	
	return reset_bus( device );
}
