/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "ide_internal.h"
#include "ide_sim.h"

#include <kernel/bus/scsi/scsi_cmds.h>
#include <nulibc/string.h>

#include "sync.h"
#include "queuing.h"
#include "channel_mgr.h"

static inline bool is_queuable( ide_device_info *device, CCB_SCSIIO *request )
{
	int opcode = request->cam_cdb[0];
	
	if( device->is_atapi || !device->CQ_enabled )
		return false;
		
	if( request->cam_tag_action == CAM_SIMPLE_QTAG )
		return false;
	
	if( opcode == SCSI_OP_READ_6 ||
		opcode == SCSI_OP_WRITE_6 ||
		opcode == SCSI_OP_READ_10 ||
		opcode == SCSI_OP_WRITE_10 )
		return true;
	else
		return false;
}


static inline void scsi_io( ide_bus_info *bus, CCB_HEADER *ccb )
{
	CCB_SCSIIO *request = (CCB_SCSIIO *)ccb;
	ide_device_info *device;
	bool queuable;
	ide_qrequest *qrequest;
	//ide_request_priv *priv;

	if( ccb->cam_target_id >= 2 || ccb->cam_target_lun != 0 )
		goto err_inv_device;

	device = bus->devices[ccb->cam_target_id];		
	if( device == NULL )
		goto err_inv_device;
	
	queuable = is_queuable( device, request );
	
	IDE_LOCK( bus );
	
	if( bus->state != ide_state_idle ) 
		goto err_bus_busy;

	if( device->free_qrequests == NULL ||
		(device->num_running_reqs > 0 && !queuable )) 
		goto err_device_busy;
	
	bus->state = ide_state_accessing;

	IDE_UNLOCK( bus );
	
	qrequest = device->free_qrequests;
	device->free_qrequests = qrequest->next;
	
	qrequest->request = request;
	qrequest->queuable = queuable;
	qrequest->running = true;

	/*priv = get_request_priv( request );
	priv->qrequest = qrequest;
	priv->device = device;*/
	
	++device->num_running_reqs;
	++bus->num_running_reqs;
	bus->active_qrequest = qrequest;

	device->exec_io( device, qrequest );
	
	return;
	
err_inv_device:
	ccb->cam_status = CAM_SEL_TIMEOUT;
	xpt->done( ccb );
	return;
	
err_bus_busy:
	//xpt->block_bus( bus->xpt_cookie );
	
	ccb->cam_status = CAM_BUS_QUEUE_FULL;
	xpt->done( ccb );
	IDE_UNLOCK( bus );
	return;
	
err_device_busy:
	//xpt->block_device( bus->xpt_cookie, ccb->cam_target_id, 0 );
		
	ccb->cam_status = CAM_DEV_QUEUE_FULL;
	xpt->done( ccb );
	IDE_UNLOCK( bus );
	return;
}

static void path_inquiry( ide_bus_info *bus, CCB_HEADER *ccb )
{
	CCB_PATHINQ *request = (CCB_PATHINQ *)ccb;
	
	SHOW_FLOW0( 3, "" );
	
	request->cam_version_num = 12;	// XXX 
	
	request->cam_hba_inquiry = PI_TAG_ABLE | PI_WIDE_16;
	
	request->cam_target_sprt = false;
	request->cam_hba_misc = 0;
	request->cam_hba_eng_cnt = 0;
	memset( request->cam_vuhba_flags, 0, sizeof( request->cam_vuhba_flags ));
	request->cam_sim_priv = SIM_PRIV;
	request->cam_async_flags = /*AC_SENT_BDR |*/	// XXX true for ATAPI ?
		AC_BUS_RESET;
		
	request->cam_hpath_id = 0;
	request->cam_initiator_id = 2;	// well, there is no initiator for IDE,
									// but according to SCSI its needed for scanning
	strncpy( request->cam_sim_vid, "NewOS", SIM_ID );
	strncpy( request->cam_hba_vid, bus->controller_name, HBA_ID );
	
	ccb->cam_status = CAM_REQ_CMP;
	xpt->done( &request->cam_ch );
}

static void scan_device( ide_bus_info *bus, int device )
{
	SHOW_FLOW0( 3, "" );
	
	schedule_synced_pc( bus, &bus->scan_bus_syncinfo, (void *)device );
	
	sem_acquire( bus->scan_device_sem, 1 );
}

static void scan_bus( ide_bus_info *bus, CCB_HEADER *ccb )
{
	int i;
	
	SHOW_FLOW0( 3, "" );
	
	for( i = 0; i < bus->controller_params.max_devices; ++i ) {
		scan_device( bus, i );
	}
	
	ccb->cam_status = CAM_REQ_CMP;
	xpt->done( ccb );
}

static void sim_action( ide_bus_info *bus, CCB_HEADER *ccb )
{	
	SHOW_FLOW( 3, "func_code=%i, device=%i:%i", (int)ccb->cam_func_code,
		(int)ccb->cam_target_id, (int)ccb->cam_target_lun );
		
	if( bus->disconnected )
		goto disconnected;
	
	switch( ccb->cam_func_code ) {
	case XPT_NOOP:
		// the only valid field is the path id
		// we are supposed to return immediately if the path is valid
		// (well, else we wouldn't be here, would we)
		ccb->cam_status = CAM_REQ_CMP;
		xpt->done( ccb );
		return;

	case XPT_SCSI_IO:
		scsi_io( bus, ccb );
		return;
		
	case XPT_GDEV_TYPE:
		// handled by XPT - this code is for safety only
		ccb->cam_status = CAM_REQ_INVALID;
		xpt->done( ccb );
		return;
		
	case XPT_PATH_INQ:
		path_inquiry( bus, ccb );
		return;

	case XPT_REL_SIMQ:
	case XPT_SASYNC_CB:
	case XPT_SDEV_TYPE:
		ccb->cam_status = CAM_REQ_INVALID;
		xpt->done( ccb );
		return;
		
	case XPT_SCAN_BUS:
		scan_bus( bus, ccb );
		return;
		
	case XPT_ABORT:
		// we cannot abort specific commands, so just ignore
		
		// fall through
	case XPT_TERM_IO:
		// same as abort
		ccb->cam_status = CAM_REQ_CMP;
		xpt->done( ccb );
		return;
		
	case XPT_RESET_BUS:
		// no, we don't want to do that
		ccb->cam_status = CAM_REQ_INVALID;
		xpt->done( ccb );
		return;
		
	case XPT_RESET_DEV:
		// XXX to do
		ccb->cam_status = CAM_REQ_INVALID;
		xpt->done( ccb );
		return;
		
	case XPT_SCAN_LUN:
		// currently not called by XPT; only SCAN BUS leads to a rescan
		// (probably we'll remove this function from public XPT access)
		ccb->cam_status = CAM_REQ_INVALID;
		xpt->done( ccb );
		return;

	case XPT_INQUIRY_PARAMS: {
		CAM_INQUIRY_PARAMS *params = (CAM_INQUIRY_PARAMS *)ccb;
		CAM_SIM_PARAMS *sim_params = &params->sim_params;
		
		sim_params->dma_alignment = bus->controller_params.dma_alignment;
		sim_params->dma_boundary = bus->controller_params.dma_boundary;
		sim_params->dma_boundary_solid = bus->controller_params.dma_boundary_solid;
		sim_params->max_sg_num = bus->controller_params.max_sg_num;
		sim_params->max_blocks = 256;
		
		ccb->cam_status = CAM_REQ_CMP;
		xpt->done( ccb );
		return; }

	default:
		// we don't support engine stuff nor target mode
		ccb->cam_status = CAM_REQ_INVALID;
		xpt->done( ccb );
		return;
	}
	
	return;
	
disconnected:
	ccb->cam_status = CAM_NO_HBA;
	xpt->done( ccb );
	return;
}

void create_sense( ide_device_info *device, scsi_sense *sense )
{
	memset( sense, 0, sizeof( *sense ));
	
	sense->error_code = SCSIS_CURR_ERROR;
	sense->sense_key = decode_sense_key( device->combined_sense );
	sense->add_sense_length = sizeof( *sense ) - 7;
	sense->asc = decode_sense_asc( device->combined_sense );
	sense->ascq = decode_sense_ascq( device->combined_sense );
	sense->sense_key_spec.raw.SKSV = 0;	// no additional info
}

void finish_request( ide_qrequest *qrequest )
{
	ide_device_info *device = qrequest->device;
	ide_bus_info *bus = device->bus;
	
	qrequest->running = false;
	qrequest->next = device->free_qrequests;
	device->free_qrequests = qrequest;
		
	--device->num_running_reqs;
	--bus->num_running_reqs;
	
	// paranoia
	bus->active_qrequest = NULL;

	access_finished( bus, device );	
	
	xpt->done( &qrequest->request->cam_ch );
}

void notify_sense( ide_qrequest *qrequest )
{
	CCB_SCSIIO *request = qrequest->request;
	ide_device_info *device = qrequest->device;
	
	if( request->cam_ch.cam_status == CAM_REQ_CMP ) {
		request->cam_ch.cam_status = CAM_REQ_CMP_ERR;
		request->cam_scsi_status = SCSI_STATUS_CHECK_CONDITION;
		
		if( (request->cam_ch.cam_flags & CAM_DIS_AUTOSENSE) == 0 ) {
			scsi_sense sense;
			int sense_len;
			
			create_sense( device, &sense );
			
			sense_len = min( request->cam_sense_len, sizeof( sense ));

			if( request->cam_sense_ptr != NULL ) {			
				memcpy( request->cam_sense_ptr, &sense, sense_len );
			
				request->cam_sense_resid = request->cam_sense_len - sense_len;
				request->cam_sense_len = sense_len;
			}
			
			request->cam_ch.cam_status |= CAM_AUTOSNS_VALID;
			
			device->combined_sense = 0;
		}
	} else
		// we destroyed old sense data anyway, so there's no way
		// to restore them if this request went seriously wrong 
		device->combined_sense = 0;
}

void finish_retry( ide_qrequest *qrequest )
{
	qrequest->request->cam_ch.cam_status = CAM_RESUBMIT;
	qrequest->device->combined_sense = 0;
	finish_request( qrequest );
}

void finish_reset_queue( ide_qrequest *qrequest )
{
	ide_bus_info *bus = qrequest->device->bus;
	
	xpt->block_bus( bus->xpt_cookie );

	finish_checksense( qrequest );
	send_abort_queue( qrequest->device );
		
	xpt->unblock_bus( bus->xpt_cookie );
}		

static void finish_norelease( ide_qrequest *qrequest )
{
	ide_device_info *device = qrequest->device;
	ide_bus_info *bus = device->bus;
	
	qrequest->running = false;
	qrequest->next = device->free_qrequests;
	device->free_qrequests = qrequest;
		
	--device->num_running_reqs;
	--bus->num_running_reqs;
	
	if( bus->active_qrequest == qrequest )
		bus->active_qrequest = NULL;

	xpt->done( &qrequest->request->cam_ch );
}

void finish_all_requests( ide_device_info *device, int cam_status )
{
	int i;
	
	if( device == NULL )
		return;
	
	xpt->block_device( device->bus->xpt_cookie, device->target_id, 0 );
	
	for( i = 0; i < device->queue_depth; ++i ) {
		ide_qrequest *qrequest = &device->qreq_array[i];
		
		if( qrequest->running ) {
			qrequest->request->cam_ch.cam_status = cam_status;
			finish_norelease( qrequest );
		}
	}
	
	xpt->unblock_device( device->bus->xpt_cookie, device->target_id, 0 );
}


static void finish_setsense( ide_device_info *device, ide_qrequest *qrequest, 
	int sense_key, int sense_asc )
{
	set_sense( device, sense_key, sense_asc );

	finish_checksense( qrequest );
}


cam_sim_interface ide_sim_interface = {
	(void (*)( cam_sim_cookie cookie, CCB_HEADER *ccb ))	sim_action,
	(void (*)( cam_sim_cookie cookie))	sim_unregistered
};

cam_for_sim_interface *xpt;
