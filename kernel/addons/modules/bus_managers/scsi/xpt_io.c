/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"
#include "xpt_io.h"

#include <kernel/debug.h>

#include "queuing.h"
#include "blocking.h"
#include "sg_mgr.h"
#include "periph_interface.h"
#include "bus_interface.h"
#include "bus_mgr.h"

static void xpt_requeue_request( CCB_SCSIIO *request )
{
    xpt_bus_info *bus = request->cam_ch.xpt_bus;
    xpt_device_info *device = request->cam_ch.xpt_device;

    request->cam_ch.xpt_state = XPT_STATE_QUEUED;

    mutex_lock( &bus->mutex );

    if ( request->xpt_ordered ) {
        --device->blocked_by_sim;
        --device->lock_count;
    }

    xpt_add_req_queue_first( request, device );

    if ( request->cam_ch.cam_status == CAM_DEV_QUEUE_FULL ) {
        xpt_set_device_overflow( device );
        xpt_remove_device_queue( device, bus );
    } else {
        xpt_set_bus_overflow( bus );
        xpt_add_device_queue_first( device, bus );
    }

    mutex_unlock( &bus->mutex );
}

static void xpt_resubmit_request( CCB_SCSIIO *request )
{
    xpt_bus_info *bus = request->cam_ch.xpt_bus;
    xpt_device_info *device = request->cam_ch.xpt_device;
    bool was_servicable, start_retry;

    request->cam_ch.xpt_state = XPT_STATE_QUEUED;

    mutex_lock( &bus->mutex );

    was_servicable = xpt_can_service_bus( bus );

    if ( request->xpt_ordered ) {
        --device->blocked_by_sim;
        --device->lock_count;
    }

    xpt_clear_device_overflow( device );
    xpt_clear_bus_overflow( bus );

    xpt_add_req_queue_first( request, device );

    if ( !device->blocked_by_sim ) {
        xpt_add_device_queue_first( device, bus );
        // previous line does nothing if already queued
        bus->waiting_devices = device;
    }

    start_retry = !was_servicable && xpt_can_service_bus( bus );

    mutex_unlock( &bus->mutex );

    if ( start_retry )
        sem_release_etc( bus->start_service, 1, SEM_FLAG_NO_RESCHED );
}

static inline void xpt_finish_request( CCB_SCSIIO *request )
{
    xpt_device_info *device = request->cam_ch.xpt_device;
    xpt_bus_info *bus = request->cam_ch.xpt_bus;
    bool was_servicable, start_retry;

    request->cam_ch.xpt_state = XPT_STATE_FINISHED;

    mutex_lock( &bus->mutex );

    was_servicable = xpt_can_service_bus( bus );

    if ( request->xpt_ordered ) {
        --device->blocked_by_sim;
        --device->lock_count;
    }

    xpt_clear_device_overflow( device );
    xpt_clear_bus_overflow( bus );

    if ( !device->blocked_by_sim && device->lock_count > 0 )
        xpt_add_device_queue_last( device, bus );

    start_retry = !was_servicable && xpt_can_service_bus( bus );

    mutex_unlock( &bus->mutex );

    if ( start_retry )
        sem_release_etc( bus->start_service, 1, SEM_FLAG_NO_RESCHED );

    if ( request->xpt_tmp_sg )
        cleanup_tmp_sg( request );

    sem_release_etc( request->cam_ch.completion_sem, 1, SEM_FLAG_NO_RESCHED );

    /*ref_lock_nonexcl_unlock( &device->lock );*/
}



int func_group_len[8] = {
    6, 10, 10, 0, 0, 12, 0, 0
};


void xpt_scsi_io( CCB_HEADER *ccb )
{
    xpt_bus_info *bus = ccb->xpt_bus;
    CCB_SCSIIO *request = (CCB_SCSIIO *)ccb;

    if ( request->cam_cdb_len < func_group_len[request->cam_cdb[0] >> 5] ) {
        ccb->cam_status = CAM_REQ_INVALID;
        xpt_done_io( ccb );
        return;
    }

    request->xpt_tmp_sg = (ccb->cam_flags & CAM_DIR_MASK) != CAM_DIR_NONE &&
                          request->cam_sg_list == NULL;

    if ( request->xpt_tmp_sg ) {
        if ( !create_temp_sg( request )) {
            // XXX this means too much (fragmented) data
            ccb->cam_status = CAM_DATA_RUN_ERR;
            xpt_done_io( ccb );
            return;
        }
    }

    // SCSI-1 uses 3 bits of command packet for LUN
    // SCSI-2 uses identify message, but still needs LUN in command packet
    //        (though it won't fit, as LUNs can be 4 bits wide)
    // SCSI-3 doesn't use command packet for LUN anymore

    // currently, we always copy LUN into command packet as a safe bet
    {
        // abuse TUR to find proper spot in command packet for LUN
        scsi_cmd_tur *cmd = (scsi_cmd_tur *)request->cam_cdb;

        cmd->LUN = ccb->xpt_device->target_lun;
    }

    if ( !bus->TCQ_support || !ccb->xpt_device->inquiry_data.CmdQue )
        request->cam_tag_action = CAM_NO_QTAG;

    request->xpt_ordered = request->cam_tag_action != CAM_SIMPLE_QTAG;

    if ( xpt_check_enqueue_request( request ))
        return;

    bus = request->cam_ch.xpt_bus;

    ccb->xpt_state = XPT_STATE_SENT;
    bus->interface->action( bus->sim_cookie, ccb );
}

void xpt_term_io( CCB_HEADER *ccb )
{
    ccb->xpt_state = XPT_STATE_SENT;
    ccb->xpt_bus->interface->action( ccb->xpt_bus->sim_cookie, ccb );
}


void xpt_abort( CCB_HEADER *ccb )
{
    CCB_ABORT *request = (CCB_ABORT *)ccb;
    CCB_HEADER *ccb_to_abort = request->cam_abort_ch;
    xpt_bus_info *bus = ccb_to_abort->xpt_bus;

    //sem_acquire( xpt_busses_lock, 1 );

    if ( bus == NULL ) {
        // checking the validity of the request to abort is a nightmare
        // this is just a beginning
        ccb->cam_status = CAM_REQ_INVALID;

    } else {
        mutex_lock( &bus->mutex );

        switch ( ccb_to_abort->xpt_state ) {
            case XPT_STATE_FINISHED:
            case XPT_STATE_SENT:
                mutex_unlock( &bus->mutex );
                break;

            case XPT_STATE_QUEUED: {
                CCB_SCSIIO *req_to_abort = (CCB_SCSIIO *)ccb_to_abort;
                xpt_device_info *device = ccb_to_abort->xpt_device;
                bool was_servicable, start_retry;

                was_servicable = xpt_can_service_bus( bus );

                xpt_remove_req_queue( req_to_abort, device );

                if ( device->queued_reqs == NULL )
                    xpt_remove_device_queue( device, bus );

                start_retry = xpt_can_service_bus( bus ) && !was_servicable;

                mutex_unlock( &bus->mutex );

                ccb_to_abort->cam_status = CAM_REQ_ABORTED;
                xpt_done( ccb_to_abort );

                if ( start_retry )
                    sem_release( bus->start_service, 1 );

                break;
            }
        }

        ccb->cam_status = CAM_REQ_CMP;
    }

    //sem_release( xpt_busses_lock, 1 );

    xpt_done( ccb );
}

bool xpt_check_exec_service( xpt_bus_info *bus )
{
    SHOW_FLOW0( 3, "" );
    mutex_lock( &bus->mutex );

    if ( xpt_can_service_bus( bus )) {
        CCB_SCSIIO *request;
        xpt_device_info *device;
        bool remove_device;

        device = bus->waiting_devices;
        bus->waiting_devices = bus->waiting_devices->next_waiting;

        request = device->queued_reqs;
        xpt_remove_req_queue( request, device );

        if ( !bus->TCQ_support )
            request->cam_tag_action = 0;

        request->xpt_ordered = request->cam_tag_action != CAM_SIMPLE_QTAG;

        if ( !request->xpt_ordered ) {
            device->last_sort = request->cam_sort;

            remove_device = device->queued_reqs == NULL;
        } else {
            ++device->blocked_by_sim;
            ++device->lock_count;
            remove_device = true;
        }

        if ( remove_device )
            xpt_remove_device_queue( device, bus );

        mutex_unlock( &bus->mutex );

        bus->resubmitted_req = request;

        request->cam_ch.xpt_state = XPT_STATE_SENT;
        bus->interface->action( bus->sim_cookie, &request->cam_ch );

        return true;
    }

    mutex_unlock( &bus->mutex );

    return false;
}

void xpt_done_io( CCB_HEADER *ccb )
{
    CCB_SCSIIO *request = (CCB_SCSIIO *)ccb;

    if ( ccb->cam_status == CAM_DEV_QUEUE_FULL ||
            ccb->cam_status == CAM_BUS_QUEUE_FULL ) {
        xpt_requeue_request( request );

    } else if ( ccb->cam_status == CAM_RESUBMIT )
        xpt_resubmit_request( request );

    else
        xpt_finish_request( request );
    return;
}
