/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "scsi_periph_int.h"

#include <string.h>

int scsi_check_capacity( scsi_device_info *device, CCB_HEADER *ccb )
{
	CCB_SCSIIO *request = (CCB_SCSIIO *)ccb;
	scsi_res_read_capacity capacity_res;
	scsi_cmd_read_capacity *cmd = (scsi_cmd_read_capacity *)request->cam_cdb;
	err_res error_res;
	uint64 capacity;
	uint32 block_size;
	
	SHOW_FLOW0( 3, "" );
	
	if( device->driver->callbacks->set_capacity == NULL )
		return NO_ERROR;
	
	request->cam_ch.cam_func_code = XPT_SCSI_IO;
	request->cam_ch.cam_flags = CAM_DIR_IN;
	
	request->cam_data = (char *)&capacity_res;
	request->cam_dxfer_len = sizeof( capacity_res );
	request->cam_cdb_len = sizeof( scsi_cmd_read_capacity );
	request->cam_timeout = device->std_timeout;
	request->cam_tag_action = CAM_NO_QTAG;
	
	memset( cmd, 0, sizeof( *cmd ));
	cmd->opcode = SCSI_OP_READ_CAPACITY;
	// we don't set PMI (partial medium indicator) as we want the whole capacity
	// in this case, all other parameters must be zero
	
	xpt->action( ccb );
	sem_acquire( ccb->completion_sem, 1 );
	
	SHOW_FLOW0( 3, "1" );
	
	error_res = check_error( device, request );
	
	SHOW_FLOW0( 3, "2" );
	
	mutex_lock( &device->mutex );

	if( error_res.action != err_act_ok || request->cam_resid != 0 ) {
		device->driver->callbacks->set_capacity( device->periph_device, 0, 0 );

		mutex_unlock( &device->mutex );
		return NO_ERROR;
	} 
	
	capacity = 
		(capacity_res.top_LBA << 24) |
		(capacity_res.high_LBA << 16) |
		(capacity_res.mid_LBA << 8) |
		capacity_res.low_LBA;
		
	block_size = 
		(capacity_res.top_block_size << 24) |
		(capacity_res.high_block_size << 16) |
		(capacity_res.mid_block_size << 8) |
		capacity_res.low_block_size;
		
	SHOW_FLOW( 3, "capacity=%Li, block_size=%i", capacity, block_size );
		
	device->driver->callbacks->set_capacity( device->periph_device, 
		capacity, block_size );
		
/*	device->byte2blk_shift = log2( device->block_size );
	if( device->byte2blk_shift < 0 ) {
		// this may be too restrictive...
		device->capacity = -1;
		return ERR_DEV_GENERAL;
	}*/
	
	mutex_unlock( &device->mutex );
	
	SHOW_FLOW0( 3, "done" );
	
	return NO_ERROR;
}
