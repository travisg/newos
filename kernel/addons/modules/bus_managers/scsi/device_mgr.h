/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __DEVICE_MGR_H__
#define __DEVICE_MGR_H__


//extern mutex xpt_devices_mutex;
extern sem_id xpt_start_rescan;

void xpt_schedule_busscan( xpt_bus_info *bus );
void xpt_get_device_type( CCB_HEADER *ccb );
//void xpt_remove_device( xpt_bus_info *bus, int target_id, int lun );
void xpt_scan_bus( CCB_HEADER *ccb );
void xpt_scan_lun( CCB_HEADER *ccb );

xpt_device_info *xpt_get_device( xpt_bus_info *bus, int target_id, int target_lun );
int xpt_put_device( xpt_device_info *device );

xpt_device_info *xpt_create_device( xpt_bus_info *bus, int target_id, int target_lun );
void xpt_destroy_device( xpt_device_info *device );

int xpt_rescan_threadproc( void *arg );

#endif
