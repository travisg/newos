/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "scsi_periph_int.h"

#include <kernel/dev/blkman.h>
#include <kernel/debug.h>

static err_res check_sense( scsi_device_info *device, CCB_SCSIIO *request )
{
	scsi_sense *sense = (scsi_sense *)request->cam_sense_ptr;
	
	if( (request->cam_ch.cam_status & CAM_AUTOSNS_VALID) == 0 ) {
		// shouldn't happen (cam_status should be CAM_AUTOSENSE_FAIL
		// as we asked for autosense)
		return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );
	}
	if( request->cam_sense_len - request->cam_sense_resid < 
		(int)offsetof( scsi_sense, add_sense_length ) + 1 ) 
	{
		// that's a bit too short
		return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );
	}
	
	if( !sense->valid ) {
		// error code not conforming to standard - too bad
		return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );
	}

	switch( sense->error_code ) {
	case SCSIS_DEFERRED_ERROR:
		// we are f**ked - some previous request turned out to have failed
		// we neither know which one nor can we resubmit it		
		dprintf( "SCSI: encountered DEFERRED ERROR - bye, bye\n" );
		return MK_ERROR( err_act_ok, NO_ERROR );
		
	case SCSIS_CURR_ERROR:
		// we start with very specific and finish very general error infos
		switch( (sense->asc << 8) | sense->ascq ) {
		case SCSIS_ASC_AUDIO_PLAYING:
			// we need something like "busy"
			return MK_ERROR( err_act_fail, ERR_NOT_ALLOWED );
			
		case SCSIS_ASC_LUN_NEED_INIT:
			// XXX at first glance, this looks like a good "medium changed"
			// indicator, but I've read somewhere that there are drives that
			// send this message if they are idle and spun down
			return MK_ERROR( err_act_start, ERR_DEV_NOT_READY );
			
		case SCSIS_ASC_LUN_NEED_MANUAL_HELP:
			return MK_ERROR( err_act_fail, ERR_DEV_NOT_READY );
			
		case SCSIS_ASC_LUN_FORMATTING:
			// we could wait, but as formatting normally takes quite long,
			// we give up without any further retries
			return MK_ERROR( err_act_fail, ERR_DEV_NOT_READY );
			
		case SCSIS_ASC_MEDIUM_CHANGED:
			media_changed( device, &request->cam_ch );
			return MK_ERROR( err_act_fail, ERR_DEV_MEDIA_CHANGED );
			
		case SCSIS_ASC_WRITE_ERR_AUTOREALLOC:
			dprintf( "Recovered write error - block got reallocated automatically\n" );
			return MK_ERROR( err_act_ok, NO_ERROR );

		case SCSIS_ASC_ID_RECOV:
			dprintf( "Recovered ID with ECC\n" );
			return MK_ERROR( err_act_ok, NO_ERROR );
			
		case SCSIS_ASC_REMOVAL_REQUESTED:
			dprintf( "Removal requested" );
			mutex_lock( &device->mutex );
			device->removal_requested = true;
			mutex_unlock( &device->mutex );
			
			return MK_ERROR( err_act_retry, ERR_DEV_GENERAL );
			
		}
		
		switch( sense->asc ) {
		case SCSIS_ASC_DATA_RECOV_NO_ERR_CORR >> 8:
		case SCSIS_ASC_DATA_RECOV_WITH_CORR >> 8:
			// these are the groups of recovered data with or without correction
			// we should print at least a warning here
			dprintf( "Recovered data, asc=0x%2x, ascq=0x%2x\n", 
				sense->asc, sense->ascq );
			return MK_ERROR( err_act_ok, NO_ERROR );
		
		case SCSIS_ASC_WRITE_PROTECTED >> 8:
			// isn't there any proper "write protected" error code?
			return MK_ERROR( err_act_fail, ERR_DEV_WRITE_ERROR );
			
		case SCSIS_ASC_NO_MEDIUM >> 8:
			return MK_ERROR( err_act_fail, ERR_DEV_NO_MEDIA );
			
		}
			
		switch( sense->sense_key ) {
		case SCSIS_KEY_NO_SENSE:
			// we thought there was an error, huh?
			return MK_ERROR( err_act_ok, NO_ERROR );

		case SCSIS_KEY_RECOVERED_ERROR:
			// we should probably tell about that; perhaps tomorrow
			return MK_ERROR( err_act_ok, NO_ERROR );
		
		case SCSIS_KEY_NOT_READY:
			return MK_ERROR( err_act_tur, ERR_DEV_NOT_READY );

		case SCSIS_KEY_MEDIUM_ERROR:
			return MK_ERROR( err_act_retry, ERR_DEV_READ_ERROR );
			
		case SCSIS_KEY_HARDWARE_ERROR:
			return MK_ERROR( err_act_retry, ERR_DEV_READ_ERROR );

		case SCSIS_KEY_ILLEGAL_REQUEST:
			return MK_ERROR( err_act_invalid_req, ERR_DEV_GENERAL );

		case SCSIS_KEY_UNIT_ATTENTION:
			return MK_ERROR( err_act_tur, ERR_DEV_NOT_READY );

		case SCSIS_KEY_DATA_PROTECT:
			// we could set "permission denied", but that's probably
			// irritating to the user
			return MK_ERROR( err_act_fail, ERR_NOT_ALLOWED );

		case SCSIS_KEY_BLANK_CHECK:
			return MK_ERROR( err_act_fail, ERR_DEV_UNREADABLE );
			
		case SCSIS_KEY_VENDOR_SPECIFIC:
			return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );

		case SCSIS_KEY_COPY_ABORTED:
			// we don't use copy, so this is really wrong
			return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );

		case SCSIS_KEY_ABORTED_COMMAND:
			// proper error code?
			return MK_ERROR( err_act_retry, ERR_DEV_GENERAL );

		case SCSIS_KEY_EQUAL:
		case SCSIS_KEY_MISCOMPARE:
			// we don't search, so this is really wrong
			return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );

		case SCSIS_KEY_VOLUME_OVERFLOW:
			// not the best return code, but this error doesn't apply
			// to devices we currently support
			return MK_ERROR( err_act_fail, ERR_DEV_SEEK_ERROR );
		
		case SCSIS_KEY_RESERVED:
		default:
			return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );
		}

		
	default:
		// shouldn't happen - there are only 2 error codes defined
		return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );
	}
}

static err_res check_scsi_status( scsi_device_info *device, CCB_SCSIIO *request )
{
	switch( request->cam_scsi_status & SCSI_STATUS_MASK ) {
	case SCSI_STATUS_GOOD:
		// shouldn't happen (cam_status should be CAM_REQ_CMP)
		return MK_ERROR( err_act_ok, NO_ERROR );
		
	case SCSI_STATUS_CHECK_CONDITION:
		return check_sense( device, request );
				
	case SCSI_STATUS_QUEUE_FULL:
		// SIM should have automatically requeued request, fall through
		
	case SCSI_STATUS_BUSY:
		// take deep breath and try again
		thread_snooze( 1000000 );
		return MK_ERROR( err_act_retry, ERR_DEV_TIMEOUT );
		
	case SCSI_STATUS_COMMAND_TERMINATED:
		return MK_ERROR( err_act_retry, ERR_DEV_GENERAL );
		
	default:
		return MK_ERROR( err_act_retry, ERR_DEV_GENERAL );	
	}
}

err_res check_error( scsi_device_info *device, CCB_SCSIIO *request )
{
	switch( request->cam_ch.cam_status & CAM_STATUS_MASK ) {
	// everything is ok
	case CAM_REQ_CMP:
		return MK_ERROR( err_act_ok, NO_ERROR );	

	// no device
	case CAM_LUN_INVALID:
	case CAM_TID_INVALID:
	case CAM_PATH_INVALID:
	case CAM_DEV_NOT_THERE:
	case CAM_NO_HBA:
		return MK_ERROR( err_act_fail, ERR_DEV_BAD_DRIVE_NUM );	
	
	// device temporary unavailable
	case CAM_SEL_TIMEOUT:
	case CAM_BUSY:
	case CAM_SCSI_BUSY:
	case CAM_HBA_ERR:
	case CAM_MSG_REJECT_REC:
	case CAM_NO_NEXUS:
	case CAM_FUNC_NOTAVAIL:
	case CAM_RESRC_UNAVAIL:
		// take a deep breath and hope device becomes ready
		thread_snooze( 1000000 );
		return MK_ERROR( err_act_retry, ERR_DEV_TIMEOUT );

	// data transmission went wrong
	case CAM_DATA_RUN_ERR:
	case CAM_UNCOR_PARITY:
		// retry immediately
		return MK_ERROR( err_act_retry, ERR_DEV_READ_ERROR );
	
	// request broken
	case CAM_REQ_INVALID:
	case CAM_CCB_LEN_ERR:
		return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );
	
	// request aborted
	case CAM_REQ_ABORTED:
	case CAM_SCSI_BUS_RESET:
	case CAM_REQ_TERMIO:
	case CAM_UNEXP_BUSFREE:
	case CAM_BDR_SENT:
	case CAM_CMD_TIMEOUT:
	case CAM_IID_INVALID:
	case CAM_UNACKED_EVENT:
	case CAM_IDE:
		// take a small breath and retry
		thread_snooze( 100000 );
		return MK_ERROR( err_act_retry, ERR_DEV_TIMEOUT );

	// device error
	case CAM_REQ_CMP_ERR:
		return check_scsi_status( device, request );
	
	// device error, but we don't know what happened
	case CAM_AUTOSENSE_FAIL:
		return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );
		
	// should not happen, give up
	case CAM_BUS_RESET_DENIED:
	case CAM_PROVIDE_FAIL:
	case CAM_UA_TERMIO:
	case CAM_CDB_RECVD:
	case CAM_LUN_ALLREADY_ENAB:
		
	default:
		return MK_ERROR( err_act_fail, ERR_DEV_GENERAL );
	}
}
