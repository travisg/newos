/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "scsi_periph_int.h"

#include <kernel/heap.h>
#include <kernel/dev/blkman.h>

int handle_open( scsi_device_info *device, periph_handle_cookie periph_handle,
	scsi_handle_info **res_handle )
{
	int res;
	scsi_handle_info *handle;
	
	handle = kmalloc( sizeof( *handle ));
	if( handle == NULL )
		return ERR_NO_MEMORY;

	mutex_lock( &device->mutex );
	
	if( !device->valid ) {
		res = ERR_DEV_BAD_DRIVE_NUM;
		goto err;
	}

	//handle->pending_error = 0;
	handle->periph_handle = periph_handle;
	
	handle->next = device->handles;
	handle->prev = NULL;

	if( device->handles )
		device->handles->prev = handle;
		
	device->handles = handle;
	
	res = NO_ERROR;

	mutex_unlock( &device->mutex );
	return res;
	
err:	
	mutex_unlock( &device->mutex );
	kfree( handle );
	return res;
}

int handle_close( scsi_handle_info *handle )
{
	scsi_device_info *device = handle->device;
	bool to_destroy;
	
	mutex_lock( &device->mutex );
	
	if( handle->next )
		handle->next->prev = handle->prev;
	if( handle->prev )
		handle->prev->next = handle->next;
	else
		device->handles = handle->next;
		
	to_destroy = !device->valid && device->handles == NULL;
	// if we are going to destroy the device, add us as a "fake"
	// handle so the device doesn't get destroyed between releasing
	// the local device lock and getting the global devices lock
	if( to_destroy )
		device->handles = handle;

	mutex_unlock( &device->mutex );
	
	if( to_destroy ) {
		mutex_lock( &periph_devices_lock );
		destroy_device( device );
		mutex_unlock( &periph_devices_lock );
	}
	
	kfree( handle );
	
	return NO_ERROR;
}
