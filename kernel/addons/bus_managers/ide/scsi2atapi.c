/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include "ide_internal.h"
#include "atapi.h"

#include <nulibc/string.h>

#include "ide_sim.h"

void atapi_exec_io( ide_device_info *device, ide_qrequest *qrequest )
{
	CCB_SCSIIO *request = qrequest->request;
	
	SHOW_FLOW0( 3, "" );
	
	start_request( device, qrequest );

	// ATAPI command packets are 12 bytes long; 
	// if the command is shorter, remaining bytes must be padded with zeros
	memset( device->packet, 0, sizeof( device->packet ));

	switch( device->device_type ) {
	case scsi_dev_CDROM:
	case scsi_dev_optical:
		// CD-ROM and optical devices don't know some 6 byte commands -
		// we translate them to their 10 byte partners
		switch( request->cam_cdb[0] ) {
		case SCSI_OP_READ_6:
		case SCSI_OP_WRITE_6: {
			scsi_cmd_rw_6 *cmd = (scsi_cmd_rw_6 *)request->cam_cdb;
			scsi_cmd_rw_10 *packet = (scsi_cmd_rw_10 *)device->packet;
			
			packet->opcode = cmd->opcode + (SCSI_OP_READ_10 - SCSI_OP_READ_6);
			packet->LUN = cmd->LUN;
			packet->low_LBA = cmd->low_LBA;
			packet->mid_LBA = cmd->mid_LBA;
			packet->high_LBA = cmd->high_LBA;
			packet->low_length = cmd->length;
			packet->high_length = cmd->length == 0;
			packet->control = cmd->control;
			break; }
			
		case SCSI_OP_MODE_SENSE_6: {
			scsi_cmd_mode_sense_6 *cmd = (scsi_cmd_mode_sense_6 *)request->cam_cdb;
			scsi_cmd_mode_sense_10 *packet = (scsi_cmd_mode_sense_10 *)device->packet;
	
			packet->opcode = SCSI_OP_MODE_SENSE_10;
			packet->DBD = cmd->DBD;
			packet->LUN = cmd->LUN;
			packet->page_code = cmd->page_code;
			packet->PC = cmd->PC;
			packet->low_allocation_length = cmd->allocation_length
				- sizeof( scsi_cmd_mode_sense_6 ) + sizeof( scsi_cmd_mode_sense_10 );
			packet->control = cmd->control;
			
			device->saved_sg_list = request->cam_sg_list;
			device->saved_sglist_cnt = request->cam_sglist_cnt;
			device->saved_dxfer_len = request->cam_dxfer_len;
		
			request->cam_sg_list = device->buffer_sg_list;
			request->cam_sglist_cnt = device->buffer_sglist_cnt;
			request->cam_dxfer_len = device->buffer_size;	
			break; }
			
		case SCSI_OP_MODE_SELECT_6: {
			scsi_cmd_mode_select_6 *cmd = (scsi_cmd_mode_select_6 *)request->cam_cdb;
			scsi_cmd_mode_select_10 *packet = (scsi_cmd_mode_select_10 *)device->packet;
			scsi_mode_param_header_6 *header_6 = (scsi_mode_param_header_6 *)request->cam_data;
			scsi_mode_param_header_10 *header_10 = (scsi_mode_param_header_10 *)device->buffer;		
			
			if( request->cam_dxfer_len < sizeof( scsi_mode_param_header_6 )) {
				// this is a bit too small, reject it
				set_sense( device, SCSIS_KEY_ILLEGAL_REQUEST, SCSIS_ASC_INV_PARAM_LIST_FIELD );
				finish_checksense( qrequest );	
				return;
			}
		
			packet->opcode = SCSI_OP_MODE_SELECT_10;
			packet->SP = cmd->SP;
			packet->PF = cmd->PF;
			packet->LUN = cmd->LUN;
			packet->low_param_list_length = cmd->param_list_length
				- sizeof( scsi_cmd_mode_sense_6 ) + sizeof( scsi_cmd_mode_sense_10 );
			packet->control = cmd->control;
			
			device->saved_sg_list = request->cam_sg_list;
			device->saved_sglist_cnt = request->cam_sglist_cnt;
			device->saved_dxfer_len = request->cam_dxfer_len;
			
			request->cam_sg_list = device->buffer_sg_list;
			request->cam_sglist_cnt = device->buffer_sglist_cnt;
			request->cam_dxfer_len = min( 
				device->buffer_size - sizeof( scsi_mode_param_header_10 ), 
				request->cam_dxfer_len - sizeof( scsi_mode_param_header_6 ))
				+ sizeof( scsi_mode_param_header_10 );
	
			memset( header_10, 0, sizeof( *header_10 ));
			// mode_data_len is reserved for MODE SELECT
			header_10->medium_type = header_6->medium_type;
			header_10->dev_spec_parameter = header_6->dev_spec_parameter;
			header_10->low_block_desc_len = header_6->block_desc_len;
			
			memcpy( 
				&device->buffer[sizeof( scsi_mode_param_header_10 )],
				&request->cam_data[sizeof( scsi_mode_param_header_6 )],
				request->cam_dxfer_len - sizeof( scsi_mode_param_header_10 ));
			
			break; }
			
		default:
			memcpy( device->packet, request->cam_cdb, request->cam_cdb_len );
		}
		break;
		
	default:
		memcpy( device->packet, request->cam_cdb, request->cam_cdb_len );
	}
	
	send_packet( device, qrequest, 
		(request->cam_ch.cam_flags & CAM_DIR_MASK) == CAM_DIR_OUT );
}

void adjust_atapi_result( ide_device_info *device, ide_qrequest *qrequest )
{
	SHOW_FLOW0( 3, "" );
	
	switch( device->device_type ) {
	case scsi_dev_CDROM:
	case scsi_dev_optical: {
		CCB_SCSIIO *request = qrequest->request;
		
		switch( (((int)device->packet[0]) << 8) | request->cam_cdb[0] ) {
		case (SCSI_OP_MODE_SENSE_10 << 8) | SCSI_OP_MODE_SENSE_6: {
			int buffer_data_len = request->cam_dxfer_len - request->cam_resid;
			
			request->cam_sg_list = device->saved_sg_list;
			request->cam_sglist_cnt = device->saved_sglist_cnt;
			request->cam_dxfer_len = device->saved_dxfer_len;
			
			if( device->combined_sense )
				return;
	
			if( buffer_data_len >= (int)sizeof( scsi_mode_param_header_10 )) {
				scsi_mode_param_header_6 *header_6 = (scsi_mode_param_header_6 *)request->cam_data;
				scsi_mode_param_header_10 *header_10 = (scsi_mode_param_header_10 *)device->buffer;
				int data_len;
				
				header_6->mode_data_len = header_10->low_mode_data_len
					- sizeof( scsi_cmd_mode_sense_10 ) + sizeof( scsi_cmd_mode_sense_6 )
					- 1;
				header_6->medium_type = header_10->medium_type;
				header_6->dev_spec_parameter = header_10->dev_spec_parameter;
				header_6->block_desc_len = header_10->low_block_desc_len;
				
				data_len = min( 
					request->cam_dxfer_len - sizeof( scsi_mode_param_header_6 ),
					buffer_data_len - sizeof( scsi_mode_param_header_10))
					+ sizeof( scsi_mode_param_header_6 );
				
				memcpy( 
					&request->cam_data[sizeof( scsi_mode_param_header_6 )],
					&device->buffer[sizeof( scsi_mode_param_header_10 )],
					data_len - sizeof( scsi_mode_param_header_6 ));
					
				request->cam_resid = request->cam_dxfer_len - data_len;
			} else
				// cheat a bit: if it is too short, report 0 bytes transferred
				request->cam_resid = request->cam_dxfer_len;
			
			break; }
			
		case (SCSI_OP_MODE_SELECT_10 << 8) | SCSI_OP_MODE_SELECT_6: {
			request->cam_sg_list = device->saved_sg_list;
			request->cam_sglist_cnt = device->saved_sglist_cnt;
			request->cam_dxfer_len = device->saved_dxfer_len;
			
			request->cam_resid += sizeof( scsi_mode_param_header_6 ) - 
				sizeof( scsi_mode_param_header_10 );
			
			break; }
			
		case (SCSI_OP_INQUIRY << 8) | SCSI_OP_INQUIRY: {
			int datalen = request->cam_dxfer_len - request->cam_resid;
			
			if( device->combined_sense )
				return;

			if( datalen >= (int)sizeof( scsi_res_inquiry )) {			
				scsi_res_inquiry *res = (scsi_res_inquiry *)request->cam_data;
			
				// pinched from linux - looks like some drives report wrong version
				res->ANSI_version = 2;
				res->response_data_format = 2;
			}
			
			break; }
			
		}
		
		break; }
	}
}
