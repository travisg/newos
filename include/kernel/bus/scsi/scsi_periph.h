/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __SCSI_PERIPH_H__
#define __SCSI_PERIPH_H__

#include <kernel/bus/scsi/CAM.h>
#include <kernel/bus/scsi/scsi_cmds.h>

typedef struct scsi_device_info *scsi_device_cookie;
typedef struct scsi_handle_info *scsi_handle_cookie;

typedef enum {
	err_act_ok,
	err_act_retry,
	err_act_fail,
	err_act_tur,
	err_act_many_retries,
	err_act_start,
	err_act_invalid_req
} err_act;

typedef struct err_res {
	uint32 action : 8;
	uint32 error_code : 24;
} err_res;

#define MK_ERROR( action, code ) ({ \
	err_res res = {action, code};	\
	res;					\
})

typedef struct periph_info *periph_cookie;
typedef struct periph_device_info *periph_device_cookie;
typedef struct periph_handle_info *periph_handle_cookie;

typedef struct scsi_periph_callbacks {
	bool (*is_supported)( periph_cookie periph, CCB_GETDEV *dev_type );
	// safe all passed information before using ccb as data is shared
	int (*device_init)( periph_cookie periph, scsi_device_cookie device,
		scsi_res_inquiry *device_inquiry, CAM_SIM_PARAMS *sim_inquiry, 
		xpt_device_cookie xpt_device, CCB_HEADER *ccb, 
		periph_device_cookie *periph_device );
	int (*device_removed)( periph_device_cookie periph_device );
	int (*device_uninit)( periph_device_cookie periph_device );
	void (*handle_set_error)( periph_handle_cookie periph_handle, int error_code );
	int (*handle_get_error)( periph_handle_cookie periph_handle );
	// set to NULL if not a block device
	void (*set_capacity)( periph_device_cookie periph_device, uint64 capacity,
		uint32 block_size );
	
	void (*async)( periph_cookie periph, 
		int path_id, int target_id, int lun, 
		int code, const void *data, int data_len );
} scsi_periph_callbacks;

typedef struct scsi_driver_info *scsi_driver_cookie;

typedef struct scsi_periph_interface {
	void (*media_changed)( scsi_device_cookie device, CCB_HEADER *ccb );
	int (*check_capacity)( scsi_device_cookie device, CCB_HEADER *ccb );
	err_res (*check_error)( scsi_device_cookie device, CCB_SCSIIO *request );
	err_res (*send_start_stop)( scsi_device_cookie device, CCB_SCSIIO *request, 
		bool start, bool with_LoEj );
	int (*get_media_status)( scsi_handle_cookie handle );
	char *(*compose_device_name)( scsi_device_cookie device, 
		const char *prefix );
	int (*handle_open)( scsi_device_cookie device, 
		periph_handle_cookie periph_handle, scsi_handle_cookie *res_handle );
	int (*handle_close)( scsi_handle_cookie handle );
	
	int (*register_driver)( scsi_periph_callbacks *callbacks, 
		periph_cookie periph_cookie, scsi_driver_cookie *driver );
	int (*unregister_driver)( scsi_driver_cookie driver );
} scsi_periph_interface;

#define SCSI_PERIPH_MODULE_NAME "generic/scsi_periph/v1"

#endif
