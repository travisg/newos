/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"
#include "blocking.h"

#include <kernel/debug.h>

#include "queuing.h"

int xpt_unblock_bus( xpt_bus_info *bus )
{
	bool was_servicable, start_retry;

	mutex_lock( &bus->mutex );
	
	was_servicable = xpt_can_service_bus( bus );

	xpt_clear_bus_overflow( bus );
	
	if( bus->blocked_by_sim > 0 ) {
		--bus->blocked_by_sim;
		--bus->lock_count;
	}
		
	start_retry = !was_servicable && xpt_can_service_bus( bus );

	mutex_unlock( &bus->mutex );
	
	if( start_retry )
		sem_release( bus->start_service, 1 );
		
	return NO_ERROR;
}

static void xpt_unblock_device_int( xpt_bus_info *bus, xpt_device_info *device, 
	bool start_service_thread )
{
	bool was_servicable, start_retry;
	
	was_servicable = xpt_can_service_bus( bus );

	xpt_clear_device_overflow( device );
	
	if( device->blocked_by_sim > 0 ) {
		--device->blocked_by_sim;
		--device->lock_count;

		// check lock_count instead of queued_reqs as we haven't got the
		// device semaphore		
		if( device->blocked_by_sim == 0 && device->lock_count > 0 )
			xpt_add_device_queue_last( device, bus );
	}

	start_retry = !was_servicable && xpt_can_service_bus( bus );

	mutex_unlock( &bus->mutex );
	
	if( start_service_thread && start_retry )
		sem_release( bus->start_service, 1 );
}

int xpt_unblock_device( xpt_bus_info *bus, int target_id, int lun )
{
	xpt_target_info *target;
	xpt_device_info *device;
	
	mutex_lock( &bus->mutex );
	
	target = bus->targets[target_id];
	
	if( target == NULL ) {
		dprintf( "Invalid target to block\n" );
		goto err;
	}
	
	device = target->devices[lun];
	
	if( device == NULL ) {
		dprintf( "Invalid device to block\n" );
		goto err;
	}

	xpt_unblock_device_int( bus, device, true );
	return NO_ERROR;
	
err:
	mutex_unlock( &bus->mutex );
	return ERR_NOT_FOUND;
}

static void xpt_unblock_device_direct( xpt_bus_info *bus, xpt_device_info *device, 
	bool start_service_thread )
{
	mutex_lock( &bus->mutex );
	
	xpt_unblock_device_int( bus, device, start_service_thread );
}

int xpt_cont_send_bus( xpt_bus_info *bus )
{
	bool was_servicable, start_retry;
	
	mutex_lock( &bus->mutex );
	
	was_servicable = xpt_can_service_bus( bus );

	xpt_clear_bus_overflow( bus );	
	
	start_retry = !was_servicable && xpt_can_service_bus( bus );
				
	mutex_unlock( &bus->mutex );
	
	if( start_retry )
		sem_release_etc( bus->start_service, 1, SEM_FLAG_NO_RESCHED );
		
	return NO_ERROR;
}

static void xpt_cont_send_device_int( xpt_bus_info *bus, xpt_device_info *device )
{
	bool was_servicable, start_retry;
	
	was_servicable = xpt_can_service_bus( bus );
	
	if( device->sim_overflow ) {			
		device->sim_overflow = false;
		--device->lock_count;
		
		if( !device->blocked_by_sim && device->lock_count > 0 )
			xpt_add_device_queue_last( device, bus );
	}

	xpt_clear_bus_overflow( bus );
	
	start_retry = !was_servicable && xpt_can_service_bus( bus );
				
	mutex_unlock( &bus->mutex );
	
	if( start_retry )
		sem_release_etc( bus->start_service, 1, SEM_FLAG_NO_RESCHED );
}

int xpt_cont_send_device( xpt_bus_info *bus, int target_id, int lun )
{
	xpt_target_info *target;
	xpt_device_info *device;
	
	mutex_lock( &bus->mutex );
	
	target = bus->targets[target_id];
	
	if( target == NULL ) {
		dprintf( "Invalid target to continue\n" );
		goto err;
	}
	
	device = target->devices[lun];
	
	if( device == NULL ) {
		dprintf( "Invalid device to continue\n" );
		goto err;
	}
	
	xpt_cont_send_device_int( bus, device );
	return NO_ERROR;

err:	
	mutex_unlock( &bus->mutex );
	return ERR_NOT_FOUND;
}

int xpt_block_bus( xpt_bus_info *bus )
{
	mutex_lock( &bus->mutex );

	++bus->blocked_by_sim;
	
	mutex_unlock( &bus->mutex );
	
	return NO_ERROR;
}

int xpt_block_device( xpt_bus_info *bus, int target_id, int lun )
{
	xpt_target_info *target;
	xpt_device_info *device;
	
	mutex_lock( &bus->mutex );
	
	if( target_id < 0 || target_id > MAX_TARGET_ID )
		goto err;
		
	target = bus->targets[target_id];
	
	if( target == NULL )
		goto err;
		
	if( lun < 0 || lun > MAX_LUN_ID )
		goto err;
	
	device = target->devices[lun];
	
	if( device == NULL ) 
		goto err;

	++device->blocked_by_sim;
	++device->lock_count;
		
	xpt_remove_device_queue( device, bus );
	
	mutex_unlock( &bus->mutex );
	return NO_ERROR;
	
err:
	mutex_unlock( &bus->mutex );
	return ERR_NOT_FOUND;
}

