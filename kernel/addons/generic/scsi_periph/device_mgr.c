/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "scsi_periph_int.h"

#include <kernel/dev/blkman.h>

#include <nulibc/string.h>
#include <nulibc/stdio.h>
#include <kernel/heap.h>

char *compose_device_name( scsi_device_info *device, const char *prefix )
{
	char name_buffer[100];
	
	sprintf( name_buffer, "%s/%i/%i/%i/raw",
		prefix, device->path_id, device->target_id, device->target_lun );
		
	return kstrdup( name_buffer );
}


static int add_device( scsi_driver_info *driver, int path_id, int target_id, int lun )
{
	CCB_GETDEV *request;
	CAM_INQUIRY_PARAMS *inquiry_req;
	int res;
	scsi_device_info *device;
	xpt_bus_cookie xpt_bus;
	xpt_device_cookie xpt_device;
	
	char inquiry_buffer[INQLEN];
	scsi_res_inquiry *inquiry = (scsi_res_inquiry *)inquiry_buffer;
	
	SHOW_FLOW0( 3, "" );
	
	mutex_lock( &periph_devices_lock );

	xpt_bus = xpt->get_bus( path_id );
	if( xpt_bus == NULL )
		return ERR_GENERAL;
		
	xpt_device = xpt->get_device( xpt_bus, target_id, lun );
	if( xpt_device == NULL ) {
		res = ERR_GENERAL;
		goto err;
	}
		
	request = (CCB_GETDEV *)xpt->ccb_alloc( xpt_device );
	if( request == NULL ) {
		res = ERR_NO_MEMORY;
		goto err1;
	}		
	
	request->cam_ch.cam_func_code = XPT_GDEV_TYPE;
	/*request->cam_ch.cam_path_id = path_id;
	request->cam_ch.cam_target_id = target_id;
	request->cam_ch.cam_target_lun = lun;*/
	request->cam_ch.cam_flags = CAM_DIR_IN;
	request->cam_inq_data = inquiry_buffer;
	
	xpt->action( &request->cam_ch );
	
	if( request->cam_ch.cam_status != CAM_REQ_CMP ) {
		res = ERR_DEV_GENERAL;
		goto err2;
	}

	if( !driver->callbacks->is_supported( driver->periph_cookie, request )) {
		res = NO_ERROR;
		goto err2;
	}
	
	SHOW_FLOW0( 3, "device supported by driver" );
	
	inquiry = (scsi_res_inquiry *)request->cam_inq_data;
	
	device = kmalloc( sizeof( *device ));
	if( device == NULL ) {
		res = ERR_NO_MEMORY;
		goto err2;
	}
	
	memset( device, 0, sizeof( *device ));	
	
	device->path_id = path_id;
	device->target_id = target_id;
	device->target_lun = lun;
	device->xpt_device = xpt_device;
	device->xpt_bus = xpt_bus;
	device->driver = driver;
	device->valid = true;
	device->removal_requested = false;
	
	memcpy( &device->inquiry, inquiry, INQLEN );
	
	inquiry_req = (CAM_INQUIRY_PARAMS *)request;
	inquiry_req->cam_ch.cam_func_code = XPT_INQUIRY_PARAMS;
	
	xpt->action( &inquiry_req->cam_ch );
	
	if( inquiry_req->cam_ch.cam_status != CAM_REQ_CMP ) {
		res = ERR_DEV_GENERAL;
		goto err3;
	}

	device->sim_params = inquiry_req->sim_params;
	
	SHOW_FLOW0( 3, "device structure ready - calling driver init" );
	
	if( (res = driver->callbacks->device_init( driver->periph_cookie, device, 
		&device->inquiry, &inquiry_req->sim_params, device->xpt_device, &request->cam_ch, 
		&device->periph_device )) != NO_ERROR )
		goto err3;
		
	device->next = driver->devices;
	driver->devices = device;
		
	xpt->ccb_free( &request->cam_ch );

	mutex_unlock( &periph_devices_lock );
	
	SHOW_FLOW0( 3, "done" );
	
	return NO_ERROR;
	
err3:
	kfree( device );
err2:
	xpt->ccb_free( &request->cam_ch );
err1:
	xpt->put_device( xpt_device );
err:
	xpt->put_bus( xpt_bus );
	
	mutex_unlock( &periph_devices_lock );
	return res;
}

static int remove_device( scsi_driver_info *driver, 
	int path_id, int target_id, int lun )
{
	scsi_device_info *device, *prev_device, *next_device;
	
	mutex_lock( &periph_devices_lock );
	
	prev_device = NULL;
	
	for( device = driver->devices; device; prev_device = device, device = next_device ) 
	{
		next_device = device->next;
		
		if( device->path_id == path_id &&
			device->target_id == target_id &&
			device->target_lun == lun ) 
		{
			scsi_handle_info *handle;
			
			if( prev_device )
				prev_device->next = device->next;
			else
				driver->devices = device->next;
				
			mutex_lock( &device->mutex );
			
			device->valid = false;

			for( handle = device->handles; handle; handle = handle->next )
				device->driver->callbacks->handle_set_error( 
					handle->periph_handle, ERR_DEV_BAD_DRIVE_NUM );
			
			driver->callbacks->device_removed( device->periph_device );
			
			if( device->handles == NULL )
				destroy_device( device );
			
			mutex_unlock( &device->mutex );
		}
	}
	
	mutex_unlock( &periph_devices_lock );
	
	return ERR_NOT_FOUND;		
}

void scsi_async( scsi_driver_info *driver, 
	int path_id, int target_id, int lun, 
	int code, const void *data, int data_len )
{
	switch( code ) {
	case AC_FOUND_DEVICE:
		add_device( driver, path_id, target_id, lun );
		break;
		
	case AC_LOST_DEVICE:
		remove_device( driver, path_id, target_id, lun );
		break;
	
	case AC_SIM_DEREGISTER:
	case AC_SIM_REGISTER:
	case AC_SENT_BDR:
	case AC_SCSI_AEN:
	case AC_UNSOL_RESEL:
	case AC_BUS_RESET:
		
		// fall through		
	default:
		driver->callbacks->async( driver->periph_cookie, path_id, target_id, lun, 
			code, data, data_len );
		break;
	}
}

void destroy_device( scsi_device_info *device )
{
	device->driver->callbacks->device_uninit( device->periph_device );
	
	xpt->put_device( device->xpt_device );
	xpt->put_bus( device->xpt_bus );
	kfree( device );
}
