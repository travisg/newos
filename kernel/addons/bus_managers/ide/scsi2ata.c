/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include "ide_internal.h"
#include "ata.h"

#include <nulibc/string.h>

#include "ide_sim.h"
#include "ide_cmds.h"
#include "basic_prot.h"

static void mode_sense_6( ide_device_info *device, ide_qrequest *qrequest )
{
	CCB_SCSIIO *request = qrequest->request;
	scsi_cmd_mode_sense_6 *cmd = (scsi_cmd_mode_sense_6 *)request->cam_cdb;
	scsi_mode_param_header_6 *param_header;
	scsi_modepage_contr *contr;
	scsi_mode_param_block_desc *block_desc;
	size_t total_length = 
		sizeof( scsi_mode_param_header_6 ) +
		sizeof( scsi_mode_param_block_desc ) +
		sizeof( scsi_modepage_contr );
	scsi_mode_param_dev_spec_da devspec = {
		res0_0 : 0,
		DPOFUA : 0,
		res0_6 : 0,
		WP : 0
	};
	
	// we answer control page requests and "all pages" requests
	// (as the latter are the same as the first)
	if( (cmd->page_code != SCSI_MODEPAGE_CONTROL &&
		 cmd->page_code != SCSI_MODEPAGE_ALL) ||
		 
		(cmd->PC != SCSI_MODE_SENE_PC_CURRENT &&
		 cmd->PC != SCSI_MODE_SENE_PC_SAVED) ||
		cmd->allocation_length < total_length ||
		request->cam_dxfer_len < total_length ) 
	{
		request->cam_ch.cam_status = CAM_REQ_INVALID;
		finish_request( qrequest );
		return;
	}

	param_header = (scsi_mode_param_header_6 *)request->cam_data;
	param_header->mode_data_len = total_length - 1;
	param_header->medium_type = 0; 		// XXX standard is a bit vague here
	param_header->dev_spec_parameter = *(uint8 *)&devspec;
	param_header->block_desc_len = sizeof( scsi_mode_param_block_desc );
	
	block_desc = (scsi_mode_param_block_desc *)(request->cam_data 
		+ sizeof( *param_header ));
	memset( block_desc, 0, sizeof( *block_desc ));
	// density is reserved (0), descriptor apply to entire medium (num_blocks=0)
	// remains the blocklen to be set
	block_desc->high_blocklen = 0;
	block_desc->med_blocklen = 512 >> 8;
	block_desc->low_blocklen = 512 & 0xff;

	contr = (scsi_modepage_contr *)(request->cam_data 
		+ sizeof( *param_header )
		+ param_header->block_desc_len );
	
	memset( contr, 0, sizeof( *contr ));
	contr->RLEC = false;
	contr->DQue = !device->CQ_enabled;
	contr->QErr = false;				// when a command fails we requeue all 
										// lost commands automagically
	contr->QAM = SCSI_QAM_UNRESTRICTED;

	request->cam_ch.cam_status = CAM_REQ_CMP;
	finish_request( qrequest );
	return;
}


static void ata_request_sense( ide_device_info *device, ide_qrequest *qrequest )
{
	CCB_SCSIIO *request = qrequest->request;
	scsi_cmd_request_sense *cmd = (scsi_cmd_request_sense *)request->cam_cdb;
	scsi_sense sense;
	int data_len;
	
	// cannot use finish_checksense here, as data is not copied into autosense buffer
	// but into normal data buffer, SCSI result is GOOD and CAM status is REQ_CMP
	
	data_len = min( request->cam_dxfer_len, cmd->alloc_length );
	data_len = min( data_len, (int)sizeof( sense ));

	if( device->combined_sense ) {
		create_sense( device, &sense );
		memcpy( request->cam_data, &sense, data_len );
	} else
		memset( request->cam_data, 0, data_len );

	request->cam_resid = request->cam_dxfer_len - data_len;
	request->cam_dxfer_len = data_len;
	
	device->combined_sense = 0;
	
	request->cam_ch.cam_status = CAM_REQ_CMP;
	finish_request( qrequest );
}


static bool mode_select_contrpage( ide_device_info *device, ide_qrequest *qrequest,
	scsi_modepage_contr *page )
{
	if( page->header.page_length != sizeof( *page ) - sizeof( page->header )) {
		set_sense( device, SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_PARAM_LIST_LENGTH_ERR );
		return false;
	}

	enable_CQ( device, !page->DQue );
	return true;
}

static void mode_select_6( ide_device_info *device, ide_qrequest *qrequest )
{
	CCB_SCSIIO *request = qrequest->request;
	scsi_cmd_mode_select_6 *cmd = (scsi_cmd_mode_select_6 *)request->cam_cdb;
	scsi_mode_param_header_6 *param_header;
	scsi_modepage_header *page_header;
	int total_length;
	int left_length;
	
	if( cmd->SP || cmd->PF != 1 ) {
		set_sense( device, SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_INV_CDB_FIELD );
		return;
	}
	
	total_length = min( request->cam_dxfer_len, cmd->param_list_length );
	
	if( total_length < (int)sizeof( scsi_mode_param_header_6 )) {
		set_sense( device, SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_PARAM_LIST_LENGTH_ERR );
		return;
	}
	
	param_header = (scsi_mode_param_header_6 *)request->cam_data;
	total_length = min( total_length, param_header->mode_data_len + 1 );
	
	left_length = total_length - param_header->block_desc_len - 
		sizeof( *param_header );
		
	page_header = (scsi_modepage_header *)(request->cam_data + 
		sizeof( *param_header ) + param_header->block_desc_len );
		
	while( left_length > 0 ) {
		int page_len;
		
		if( left_length < (int)sizeof( scsi_modepage_header )) {
			set_sense( device, 
				SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_PARAM_LIST_LENGTH_ERR );
			return;
		}
			
		page_len = page_header->page_length + sizeof( scsi_modepage_header );
		if( left_length < page_len ) {
			set_sense( device, 
				SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_PARAM_LIST_LENGTH_ERR );
			return;
		}

		switch( page_header->page_code ) {
		case SCSI_MODEPAGE_CONTROL:
			if( !mode_select_contrpage( device, qrequest,
				(scsi_modepage_contr *)page_header ))
				return;
			break;
			
		default:
			set_sense( device, 
				SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_INV_PARAM_LIST_FIELD );
			return;
		}
		
		page_header = (scsi_modepage_header *)(((char *)page_header) + page_len);
		left_length -= page_len;
	}

	// plain paranoia, we shouldn't arrive here
	set_sense( device, 
		SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_PARAM_LIST_LENGTH_ERR );
}

static bool ata_tur( ide_device_info *device, ide_qrequest *qrequest )
{
	SHOW_FLOW0( 3, "" );
	
	if( !device->infoblock.RMSN_supported ||
		device->infoblock._127_RMSN_support != 1 )
		return true;
		
	// 5322
	device->tf_param_mask = 0;
	device->tf.write.command = IDE_CMD_GET_MEDIA_STATUS;
	
	if( !send_command( device, true, 15000000, ide_state_sync_waiting )) 
		return false;

	// bits ide_error_mcr | ide_error_mc | ide_error_wp are also valid
	// but not requested by TUR
	return check_output( device, true, ide_error_nm | ide_error_abrt, false );
}

static bool ata_flush_cache( ide_device_info *device, ide_qrequest *qrequest )
{
	ide_bus_info *bus = device->bus;
	
	// we should ask for FLUSH CACHE support, but everyone denies it
	// (looks like they cheat to gain some performance advantage, but
	//  that's pretty useless: everyone does it...)
	if( !device->infoblock.write_cache_supported )
		return true;
	
	device->tf_param_mask = 0;
	device->tf.lba.command = device->use_48bits ? IDE_CMD_FLUSH_CACHE_EXT 
		: IDE_CMD_FLUSH_CACHE;
	
	// spec says that this may take more then 30s, how much more?
	if( !send_command( device, true, 60000000, ide_state_sync_waiting ))
		return false;
	
	sem_acquire( bus->sync_wait_sem, 1 );

	return check_output( device, true, ide_error_abrt, false );
}

static bool ata_load_eject( ide_device_info *device, ide_qrequest *qrequest, bool load )
{
	if( load ) {
		set_sense( device, SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_PARAM_NOT_SUPPORTED );
		return false;
	}
	
	device->tf_param_mask = 0;
	device->tf.lba.command = IDE_CMD_MEDIA_EJECT;
	
	if( !send_command( device, true, 15000000, ide_state_sync_waiting ))
		return false;
		
	sem_acquire( device->bus->sync_wait_sem, 1 );
	
	return check_output( device, true, ide_error_abrt | ide_error_nm, false );
}

static bool ata_prevent_allow( ide_device_info *device, bool prevent )
{
	set_sense( device, SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_ILL_FUNCTION );
	return false;
}

static void ata_inquiry( ide_device_info *device, ide_qrequest *qrequest )
{
	CCB_SCSIIO *request = qrequest->request;
	scsi_res_inquiry *data = (scsi_res_inquiry *)request->cam_data;
	scsi_cmd_inquiry *cmd = (scsi_cmd_inquiry *)request->cam_cdb;
	
	SHOW_FLOW0( 3, "" );
	
	if( cmd->EVPD || cmd->page_code ) {
		set_sense( device, 
			SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_INV_CDB_FIELD );
		return;
	}
	
	SHOW_FLOW0( 3, "1" );
	
	if( request->cam_dxfer_len < 36 ) {
		// we could return truncated data, but it's probably
		// not worth the hazzle
		request->cam_ch.cam_status = CAM_REQ_INVALID;
		return;
	}
	
	SHOW_FLOW0( 3, "2" );
					
	memset( request->cam_data, 0, request->cam_dxfer_len );
	
	data->device_type = scsi_dev_direct_access;
	data->device_qualifier = scsi_periph_qual_connected;
	
	data->device_type_modifier = 0;
	data->RMB = false;
	
	data->ANSI_version = 2;
	data->ECMA_version = 0;
	data->ISO_version = 0;
	
	data->response_data_format = 2;
	data->TrmIOP = false;		// to be changed if we support TERM I/O

	data->additional_length = sizeof( scsi_res_inquiry ) - 4;
	
	data->SftRe = false;
	data->CmdQue = device->CQ_supported;
	data->Linked = false;
	
	// these values are free-style
	data->Sync = false;
	data->WBus16 = true;
	data->WBus32 = false;
	
	data->RelAdr = false;
	
	// the following fields are *much* to small, sigh...
	memcpy( data->vendor_ident, device->infoblock.model_number, 8 );
	memcpy( data->product_ident, device->infoblock.model_number + 8, 16 );
	memcpy( data->product_rev, "    ", 4 );
	
	request->cam_resid = request->cam_dxfer_len - sizeof( scsi_res_inquiry );
}


static void read_capacity( ide_device_info *device, ide_qrequest *qrequest )
{
	CCB_SCSIIO *request = qrequest->request;
	scsi_res_read_capacity *data = (scsi_res_read_capacity *)request->cam_data;
	scsi_cmd_read_capacity *cmd = (scsi_cmd_read_capacity *)request->cam_cdb;
		
	if( request->cam_dxfer_len < 4 ) {
		// there is no point supporting short data
		request->cam_ch.cam_status = CAM_REQ_INVALID;
		return;
	}
		
	if( cmd->PMI || cmd->top_LBA || cmd->high_LBA || cmd->mid_LBA || cmd->low_LBA ) 
	{
		set_sense( device, 
			SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_INV_CDB_FIELD );
		return;
	}
		
	data->top_block_size = (512 >> 24) & 0xff;
	data->high_block_size = (512 >> 16) & 0xff;
	data->mid_block_size = (512 >> 8) & 0xff;
	data->low_block_size = 512 & 0xff;
	
	data->top_LBA = (device->total_sectors >> 24) & 0xff;
	data->high_LBA = (device->total_sectors >> 16) & 0xff;
	data->mid_LBA = (device->total_sectors >> 8) & 0xff;
	data->low_LBA = device->total_sectors & 0xff;
	
	request->cam_resid = request->cam_dxfer_len - sizeof( *data );
}

void ata_exec_io( ide_device_info *device, ide_qrequest *qrequest )
{
	CCB_SCSIIO *request = qrequest->request;
	
	SHOW_FLOW( 3, "command=%x", request->cam_cdb[0] );

	if( request->cam_cdb[0] != SCSI_OP_REQUEST_SENSE )	
		start_request( device, qrequest );
	
	switch( request->cam_cdb[0] ) {
	case SCSI_OP_TUR:
		ata_tur( device, qrequest );
		break;

	case SCSI_OP_REQUEST_SENSE:
		ata_request_sense( device, qrequest );
		return;
		
	case SCSI_OP_FORMAT: /* FORMAT UNIT */
		// we could forward request to disk, but modern disks cannot
		// be formatted anyway, so we just refuse request
		// (exception are removable media devices, but to my knowledge
		// they don't have to be formatted as well)
		request->cam_ch.cam_status = CAM_REQ_INVALID;
		break;;

	case SCSI_OP_INQUIRY: 
		ata_inquiry( device, qrequest );
		break;;

	case SCSI_OP_MODE_SELECT_6:
		mode_select_6( device, qrequest );
		break;;
			
	case SCSI_OP_MODE_SENSE_6:
		mode_sense_6( device, qrequest );
		break;;
	
	case SCSI_OP_MODE_SELECT_10:
	case SCSI_OP_MODE_SENSE_10:
		/// XXX to do
		request->cam_ch.cam_status = CAM_REQ_INVALID;
		break;
		
	case SCSI_OP_RESERVE:
	case SCSI_OP_RELEASE:
		// though mandatory, this doesn't make much sense in a
		// single initiator environment
		request->cam_ch.cam_status = CAM_REQ_INVALID;
		break;
		
	case SCSI_OP_START_STOP: {
		scsi_cmd_ssu *cmd = (scsi_cmd_ssu *)request->cam_cdb;
		//ide_qrequest *qrequest = get_request_priv( request )->qrequest;
		
		// with no LoEj bit set, we should only allow/deny further access
		// we ignore that
		// with LoEj bit set, we should additionally either load or eject the medium
		// (start = 0 - eject; start = 1 - load)
		
		if( !cmd->start )
			// we must flush cache if start = 0
			ata_flush_cache( device, qrequest );
		
		if( cmd->LoEj )
			ata_load_eject( device, qrequest, cmd->start );
			
		break; }
		
	case SCSI_OP_PREVENT_ALLOW: {
		scsi_cmd_prevent_allow *cmd = (scsi_cmd_prevent_allow *)request->cam_cdb;
		
		ata_prevent_allow( device, cmd->prevent );
		break; }
	
	case SCSI_OP_READ_CAPACITY:
		read_capacity( device, qrequest );
		break;		
	
	case SCSI_OP_VERIFY:
		// does anyone uses this function?
		// it effectly does a read-and-compare, which IDE doesn't support
		request->cam_ch.cam_status = CAM_REQ_INVALID;
		break;
	
	case SCSI_OP_SYNCHRONIZE_CACHE:
		// we ignore range and immediate bit, we always immediately flush everything 
		ata_flush_cache( device, qrequest );
		break;
	
	// sadly, there are two possible read/write operation codes;
	// at least, the third one, read/write(12), is not valid for DAS
	case SCSI_OP_READ_6:
	case SCSI_OP_WRITE_6: {
		scsi_cmd_rw_6 *cmd = (scsi_cmd_rw_6 *)request->cam_cdb;
		uint32 pos;
		size_t length;
		
		pos = ((uint32)cmd->high_LBA << 16) | ((uint32)cmd->mid_LBA << 8)
			| (uint32)cmd->low_LBA;
			
		length = cmd->length != 0 ? cmd->length : 256;
		
		ata_send_rw( device, qrequest, pos, length, cmd->opcode == SCSI_OP_WRITE_6 );		
		return;
	}
	case SCSI_OP_READ_10:
	case SCSI_OP_WRITE_10: {
		scsi_cmd_rw_10 *cmd = (scsi_cmd_rw_10 *)request->cam_cdb;
		uint32 pos;
		size_t length;
		
		pos = ((uint32)cmd->top_LBA << 24) | ((uint32)cmd->high_LBA << 16) 
			| ((uint32)cmd->mid_LBA << 8) | (uint32)cmd->low_LBA;
			
		length = ((uint32)cmd->high_length << 8) | cmd->low_length;
		
		if( length != 0 ) {
			ata_send_rw( device, qrequest, pos, length, cmd->opcode == SCSI_OP_WRITE_10 );
		} else {
			// we cannot transfer zero bytes (apart from LBA48)
			finish_request( qrequest );
		}
		return;	}
	default:
		set_sense( device, 
			SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_INV_OPCODE );
	}
	
	finish_checksense( qrequest );
}
