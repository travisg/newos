/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __IDE_SIM_H__
#define __IDE_SIM_H__


#include <kernel/bus/scsi/scsi_cmds.h>

extern cam_for_sim_interface *xpt;
extern cam_sim_interface ide_sim_interface;


static inline void set_sense( ide_device_info *device, int sense_key, int sense_asc )
{
	device->combined_sense = (sense_key << 16) | sense_asc;
}

static inline uint8 decode_sense_key( uint32 combined_sense )
{
	return (combined_sense >> 16) & 0xff;
}

static inline uint8 decode_sense_asc( uint32 combined_sense )
{
	return (combined_sense >> 8) & 0xff;
}

static inline uint8 decode_sense_ascq( uint32 combined_sense )
{
	return combined_sense & 0xff;
}

static inline uint16 decode_sense_asc_ascq( uint32 combined_sense )
{
	return combined_sense & 0xffff;
}

void finish_request( ide_qrequest *qrequest );
void finish_reset_queue( ide_qrequest *qrequest );
void finish_retry( ide_qrequest *qrequest );
void finish_all_requests( ide_device_info *device, int cam_status );
void notify_sense( ide_qrequest *qrequest );

static inline void finish_checksense( ide_qrequest *qrequest )
{
	if( qrequest->device->combined_sense )
		notify_sense( qrequest );
	
	finish_request( qrequest );
}

static inline void start_request( ide_device_info *device, ide_qrequest *qrequest )
{
	device->combined_sense = 0;
	qrequest->request->cam_scsi_status = SCSI_STATUS_GOOD;
	qrequest->request->cam_ch.cam_status = CAM_REQ_CMP;
}


void create_sense( ide_device_info *device, scsi_sense *sense );


#endif
