/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __SCSI_PERIPH_INT_H__
#define __SCSI_PERIPH_INT_H__

#include <kernel/bus/scsi/scsi_periph.h>
#include <kernel/lock.h>

#define debug_level_flow 3
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "SCSI_PERIPH -- "

#include <kernel/debug.h>
#include <kernel/debug_ext.h>


#ifndef offsetof
#define offsetof(type, member) ((size_t)&(((type *)0)->member))
#endif

typedef struct scsi_device_info {
	struct scsi_device_info *next;
	struct scsi_handle_info *handles;
	
	xpt_device_cookie xpt_device;
	xpt_bus_cookie xpt_bus;
	periph_device_cookie periph_device;
	
	uint8 valid : 1;
	uint8 removal_requested : 1;

	uchar path_id, target_id, target_lun;
	scsi_res_inquiry inquiry;
	//char *name;
	mutex mutex;
	int std_timeout;

	/*uint64 capacity;
	uint32 block_size;*/
	
	CAM_SIM_PARAMS sim_params;
	
	struct scsi_driver_info *driver;
} scsi_device_info;

typedef struct scsi_handle_info {
	struct scsi_device_info *device;
	struct scsi_handle_info *next, *prev;

	periph_handle_cookie periph_handle;
} scsi_handle_info;

typedef struct scsi_driver_info {
	scsi_periph_callbacks *callbacks;
	periph_cookie periph_cookie;
	scsi_device_info *devices;
	xpt_periph_cookie xpt_cookie;
} scsi_driver_info;

cam_for_driver_interface *xpt;
mutex periph_devices_lock;

void media_changed( scsi_device_info *device, CCB_HEADER *ccb );
err_res check_error( scsi_device_info *device, CCB_SCSIIO *request );
int handle_open( scsi_device_info *device, periph_handle_cookie periph_handle,
	scsi_handle_info **res_handle );
int handle_close( scsi_handle_info *handle );

int scsi_register_driver( scsi_periph_callbacks *callbacks, 
	periph_cookie periph_cookie, scsi_driver_info **driver_out );
int scsi_unregister_driver( scsi_driver_info *driver );

void scsi_async( scsi_driver_info *driver, 
	int path_id, int target_id, int lun, 
	int code, const void *data, int data_len );
	
int scsi_check_capacity( scsi_device_info *device, CCB_HEADER *ccb );

void destroy_device( scsi_device_info *device );
int scsi_get_media_status( scsi_handle_info *handle );
err_res send_start_stop( scsi_device_info *device, CCB_SCSIIO *request, 
	bool start, bool with_LoEj );
int scsi_check_capacity( scsi_device_info *device, CCB_HEADER *ccb );
int handle_open( scsi_device_info *device, periph_handle_cookie periph_handle,
	scsi_handle_info **res_handle );
int handle_close( scsi_handle_cookie handle );

char *compose_device_name( scsi_device_info *device, const char *prefix );
	
#endif
