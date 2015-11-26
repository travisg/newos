/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __QUEUING_H__
#define __QUEUING_H__

void xpt_add_device_queue_last( xpt_device_info *device, xpt_bus_info *bus );
void xpt_remove_device_queue( xpt_device_info *device, xpt_bus_info *bus );
void xpt_add_req_queue_first( CCB_SCSIIO *request, xpt_device_info *device );
void xpt_add_device_queue_first( xpt_device_info *device, xpt_bus_info *bus );
void xpt_remove_req_queue( CCB_SCSIIO *request, xpt_device_info *device );

void xpt_add_queued_request( CCB_SCSIIO *request );

static inline bool xpt_check_enqueue_request( CCB_SCSIIO *request )
{
    xpt_bus_info *bus = request->cam_ch.xpt_bus;
    xpt_device_info *device = request->cam_ch.xpt_device;

    mutex_lock( &bus->mutex );

    if ( device->lock_count > 0 || bus->lock_count > 0 ) {
        xpt_add_queued_request( request );
        mutex_unlock( &bus->mutex );
        return true;

    } else {
        if ( !request->xpt_ordered ) {
            device->last_sort = request->cam_sort;
        } else {
            ++device->blocked_by_sim;
            ++device->lock_count;
        }

        mutex_unlock( &bus->mutex );
        return false;
    }
}


#endif
