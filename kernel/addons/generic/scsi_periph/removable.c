/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "scsi_periph_int.h"

#include <kernel/dev/blkman.h>
#include <string.h>

static err_res wait_for_ready( scsi_device_info *device, CCB_SCSIIO *request );

void media_changed( scsi_device_info *device, CCB_HEADER *ccb )
{
	scsi_handle_info *handle;
	
	mutex_lock( &device->mutex );
	
	for( handle = device->handles; handle; handle = handle->next )
		device->driver->callbacks->handle_set_error( 
			handle->periph_handle, ERR_DEV_BAD_DRIVE_NUM );
		
	mutex_unlock( &device->mutex );

	// TBD
	scsi_check_capacity( device, ccb );
}

int scsi_get_media_status( scsi_handle_info *handle )
{
	scsi_device_info *device = handle->device;
	CCB_SCSIIO *request;
	err_res res;
	int retries = 0;
	int err;
	
	mutex_lock( &device->mutex );
	
	if( device->removal_requested ) {
		device->removal_requested = false;
		err = ERR_DEV_MEDIA_CHANGE_REQUESTED;
		goto err;
	}

	err = device->driver->callbacks->handle_get_error( handle->periph_handle );
	if( err != NO_ERROR ) {
		device->driver->callbacks->handle_set_error( handle->periph_handle, NO_ERROR );
		goto err;
	}
	
	mutex_unlock( &device->mutex );
	
	request = (CCB_SCSIIO *)xpt->ccb_alloc( device->xpt_device );
	if( request == NULL )
		return ERR_NO_MEMORY;

	wait_for_ready( device, request );
	
	xpt->ccb_free( &request->cam_ch );
	
	return res.error_code;	
	
err:	
	mutex_unlock( &device->mutex );
	return err;
}

err_res send_start_stop( scsi_device_info *device, CCB_SCSIIO *request, 
	bool start, bool with_LoEj )
{
	scsi_cmd_ssu *cmd = (scsi_cmd_ssu *)request->cam_cdb;
	
	request->cam_ch.cam_func_code = XPT_SCSI_IO;
	request->cam_ch.cam_flags = CAM_DIR_NONE;
	
	request->cam_data = NULL;
	request->cam_sg_list = NULL;
	request->cam_dxfer_len = 0;
	request->cam_timeout = device->std_timeout;
	request->cam_tag_action = CAM_ORDERED_QTAG;
	
	memset( cmd, 0, sizeof( *cmd ));
	cmd->opcode = SCSI_OP_START_STOP;
	// we don't want to poll; we give a large timeout instead
	cmd->immed = 1;
	cmd->start = start;
	cmd->LoEj = with_LoEj;

	request->cam_cdb_len = sizeof( *cmd );
	
	xpt->action( &request->cam_ch );
	
	sem_acquire( request->cam_ch.completion_sem, 1 );

	return check_error( device, request );
}

static err_res send_tur( scsi_device_info *device, CCB_SCSIIO *request )
{
	scsi_cmd_tur *cmd = (scsi_cmd_tur *)request->cam_cdb;
	
	request->cam_ch.cam_func_code = XPT_SCSI_IO;
	request->cam_ch.cam_flags = CAM_DIR_NONE;
	
	request->cam_data = NULL;
	request->cam_sg_list = NULL;
	request->cam_dxfer_len = 0;
	request->cam_timeout = device->std_timeout;
	request->cam_tag_action = CAM_ORDERED_QTAG;
	
	memset( cmd, 0, sizeof( *cmd ));
	cmd->opcode = SCSI_OP_TUR;
	
	request->cam_cdb_len = sizeof( *cmd );
	
	xpt->action( &request->cam_ch );
	
	sem_acquire( request->cam_ch.completion_sem, 1 );
	return check_error( device, request );
}

err_res wait_for_ready( scsi_device_info *device, CCB_SCSIIO *request )
{	
	int retries = 0;
	
	while( 1 ) {
		err_res res;
		
		res = send_tur( device, request );
		
		switch( res.action ) {
		case err_act_ok:
			return MK_ERROR( err_act_ok, NO_ERROR );
			
		case err_act_retry:
		case err_act_tur:
			if( ++retries >= 3 )
				return MK_ERROR( err_act_fail, res.error_code );
			break;
			
		case err_act_many_retries:
			if( ++retries >= 30 )
				return MK_ERROR( err_act_fail, res.error_code );
			break;
		}

		thread_snooze( 500000 );

	};		
}
