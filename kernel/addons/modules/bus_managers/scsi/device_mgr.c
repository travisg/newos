/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

// TODO: there's a possible dead-lock during bus-scan and lun-scan if
// all CCBs are allocated as the scan needs an extra CCB

#include "xpt_internal.h"
#include "device_mgr.h"

#include <kernel/heap.h>
#include <string.h>

#include "blocking.h"
#include "periph_interface.h"
#include "ccb_mgr.h"
#include "async.h"
#include "bus_interface.h"
#include "bus_mgr.h"

//mutex xpt_devices_mutex;
sem_id xpt_start_rescan;


typedef enum {
    xpt_scan_res_unchanged,
    xpt_scan_res_replaced,
    xpt_scan_res_removed,
    xpt_scan_res_new
} xpt_scan_res;


void xpt_destroy_device( xpt_device_info *device )
{
    kfree( device );
}


int xpt_put_device( xpt_device_info *device )
{
    SHOW_FLOW( 3, "target=%i, lun=%i", device->target_id, device->target_lun );
    recursive_lock_lock( &registration_lock );

    if ( --device->ref_count == 0 && device->temporary ) {
        xpt_target_info *target;

        SHOW_FLOW0( 3, "cleaning up device" );

        target = device->bus->targets[device->target_id];

        target->devices[device->target_lun] = NULL;

        if ( --target->num_devices == 0 ) {
            device->bus->targets[device->target_id] = NULL;
            kfree( target );
        }

        xpt_destroy_device( device );
    }

    recursive_lock_unlock( &registration_lock );

    return NO_ERROR;
}


xpt_device_info *xpt_create_device( xpt_bus_info *bus, int target_id, int target_lun )
{
    xpt_device_info *device;

    device = kmalloc( sizeof( *device ));

    memset( device, 0, sizeof( *device ));

    device->lock_count = 0;
    device->blocked_by_sim = 0;
    device->sim_overflow = 0;
    device->queued_reqs = NULL;
    device->bus = bus;
    device->target_id = target_id;
    device->target_lun = target_lun;
    device->temporary = true;
    device->ref_count = 0;

    return device;
}

xpt_device_info *xpt_get_device( xpt_bus_info *bus, int target_id, int target_lun )
{
    xpt_target_info *target;
    xpt_device_info *device;

    SHOW_FLOW( 3, "target=%i, lun=%i", target_id, target_lun );

    if ( target_id == XPT_BUS_GLOBAL_ID && target_lun == XPT_BUS_GLOBAL_ID )
        return bus->global_device;

    if ( target_id > MAX_TARGET_ID || target_lun > MAX_LUN_ID )
        return NULL;

    recursive_lock_lock( &registration_lock );

    target = bus->targets[target_id];
    if ( target == NULL ) {
        SHOW_FLOW0( 3, "adding target" );
        target = kmalloc( sizeof( *target ));

        memset( target, 0, sizeof( *target ));
        target->num_devices = 0;

        if ( target == NULL )
            goto err2;

        bus->targets[target_id] = target;
    }

    device = target->devices[target_lun];
    if ( device == NULL ) {
        SHOW_FLOW0( 3, "adding device" );
        device = xpt_create_device( bus, target_id, target_lun );
        if ( device == NULL )
            goto err2;

        target->devices[target_lun] = device;
        ++target->num_devices;
    }

    ++device->ref_count;

    recursive_lock_unlock( &registration_lock );
    return device;

err2:
    if ( target->num_devices == 0 ) {
        bus->targets[target_id] = NULL;
        kfree( target );
    }
    recursive_lock_unlock( &registration_lock );
    return NULL;
}

static xpt_scan_res xpt_scan_send_tur( CCB_SCSIIO *worker_req )
{
    scsi_cmd_tur *cmd = (scsi_cmd_tur *)worker_req->cam_cdb;

    SHOW_FLOW0( 3, "" );

    memset( cmd, 0, sizeof( *cmd ));
    cmd->opcode = SCSI_OP_TUR;

    worker_req->cam_sg_list = NULL;
    worker_req->cam_data = NULL;
    worker_req->cam_dxfer_len = 0;
    worker_req->cam_cdb_len = sizeof( *cmd );
    worker_req->cam_timeout = SCSI_STD_TIMEOUT;

    xpt_action( &worker_req->cam_ch );
    sem_acquire( worker_req->cam_ch.completion_sem, 1 );

    SHOW_FLOW( 3, "cam_status=%x", worker_req->cam_ch.cam_status );

    // as this command was only for syncing, we ignore almost all errors
    switch ( worker_req->cam_ch.cam_status ) {
        case CAM_SEL_TIMEOUT:
            // there seems to be no device around
            return xpt_scan_res_removed;

        default:
            return xpt_scan_res_unchanged;
    }
}

static xpt_scan_res xpt_scan_send_inquiry( CCB_SCSIIO *worker_req,
        scsi_res_inquiry *new_inquiry_data/*, bool was_existing*/ )
{
    scsi_cmd_inquiry *cmd = (scsi_cmd_inquiry *)worker_req->cam_cdb;

    SHOW_FLOW0( 3, "" );

    cmd->opcode = SCSI_OP_INQUIRY;
    cmd->LUN = worker_req->cam_ch.xpt_device->target_lun;
    cmd->EVPD = 0;
    cmd->page_code = 0;
    cmd->allocation_length = sizeof( *new_inquiry_data );

    worker_req->cam_sg_list = NULL;
    worker_req->cam_data = (uchar *)new_inquiry_data;
    worker_req->cam_dxfer_len = sizeof( *new_inquiry_data );
    worker_req->cam_cdb_len = 6;
    worker_req->cam_sglist_cnt = 0;
    worker_req->cam_timeout = SCSI_STD_TIMEOUT;

    xpt_action( &worker_req->cam_ch );
    sem_acquire( worker_req->cam_ch.completion_sem, 1 );

    switch ( worker_req->cam_ch.cam_status ) {
        case CAM_REQ_CMP: {
            bool was_existing, is_existing;
            char vendor[9], product[17], rev[5];

            SHOW_FLOW0( 3, "send successfully" );

            strlcpy( vendor, new_inquiry_data->vendor_ident, sizeof( vendor ));
            strlcpy( product, new_inquiry_data->product_ident, sizeof( product ));
            strlcpy( rev, new_inquiry_data->product_rev, sizeof( rev ));
            SHOW_INFO( 3, "%s", vendor );

            SHOW_INFO( 3, "device type: %i, removable: %i, ANSI version: %i, \n\
vendor: %s, product:%s, rev: %s", new_inquiry_data->device_type,
                       new_inquiry_data->RMB, new_inquiry_data->ANSI_version,
                       vendor, product, rev );

            {
                unsigned int i;

                for ( i = 0; i < worker_req->cam_dxfer_len - worker_req->cam_resid; ++i ) {
                    dprintf( "%2x ", *((char *)new_inquiry_data + i) );
                }

                dprintf( "\n" );
            }

            // we could check transmission length here, but as we reset
            // missing bytes before, we get kind of valid data anyway (hopefully)
            was_existing = !worker_req->cam_ch.xpt_device->temporary;
            is_existing = new_inquiry_data->device_qualifier == scsi_periph_qual_connected;

            if ( was_existing != is_existing ) {
                SHOW_FLOW0( 3, "status has changed" );

                if ( is_existing )
                    return xpt_scan_res_new;
                else
                    return xpt_scan_res_removed;
            } else {
                SHOW_FLOW0( 3, "status has not changed" );

                if ( was_existing && is_existing &&
                        memcmp( new_inquiry_data, &worker_req->cam_ch.xpt_device->inquiry_data,
                                sizeof( *new_inquiry_data )) != 0 )
                    return xpt_scan_res_replaced;
                else
                    return xpt_scan_res_unchanged;
            }
            break;
        }

        default:
            return xpt_scan_res_removed;
    }
}


void xpt_scan_lun( CCB_HEADER *ccb )
{
    CCB_SCSIIO *worker_req;
    scsi_res_inquiry new_inquiry_data;
    xpt_scan_res scan_res;
    //xpt_bus_info *bus = ccb->xpt_bus;
    xpt_device_info *device = ccb->xpt_device;
    //bool temp_device;

    // the semaphore makes sure that two concurrent scans don't interfere
    sem_acquire( device->bus->scan_lun_lock, 1 );

    SHOW_FLOW0( 3, "" );

    /*  if( !xpt_make_temp_device( ccb, &temp_device ))
            goto err;*/

    //SHOW_FLOW( 3, "temp_device: %i", (int)temp_device );

    worker_req = (CCB_SCSIIO *)xpt_alloc_ccb( device );

    if ( worker_req == NULL ) {
        // XXX what is the proper return code for "out of memory"?
        ccb->cam_status = CAM_BUSY;
        goto err;
    }

    SHOW_FLOW0( 3, "2" );

    // in case not whole structure gets transferred, we set remaining data to zero
    memset( &new_inquiry_data, 0, sizeof( new_inquiry_data ));

    // we should pass this to SIM, but this would break the locking system
    // (and I don't know whether sending this ccb to SIM even makes sense)
    //ccb->xpt_bus->interface->action( ccb );
    //ccb->cam_status = CAM_REQ_INPROG;

    worker_req->cam_ch.cam_func_code = XPT_SCSI_IO;
    //worker_req->cam_ch.cam_path_id = ccb->cam_path_id;
    //worker_req->cam_ch.cam_target_id = ccb->cam_target_id;
    //worker_req->cam_ch.cam_target_lun = ccb->cam_target_lun;
    worker_req->cam_ch.cam_flags = CAM_DIR_IN;

    // to give controller a chance for transfer speed negotiation, we
    // send a TUR first; unfortunatily, some devices don't like TURing
    // invalid luns apart from lun 0...
    if ( device->target_lun == 0 ) {
        scan_res = xpt_scan_send_tur( worker_req );
        SHOW_FLOW( 3, "TUR: %i", scan_res );
    } else
        scan_res = xpt_scan_res_unchanged;

    if ( scan_res != xpt_scan_res_removed )
        scan_res = xpt_scan_send_inquiry( worker_req, &new_inquiry_data/*, !temp_device*/ );

    SHOW_FLOW0( 3, "4" );

    switch ( scan_res ) {
        case xpt_scan_res_removed:
            SHOW_FLOW0( 3, "removed" );

            if ( !device->temporary ) {
                xpt_call_async_all( device->bus, device->target_id,
                                    device->target_lun, AC_LOST_DEVICE, NULL, 0, true );
            }

            device->temporary = true;
            //xpt_remove_device( ccb->xpt_bus, ccb->cam_target_id, ccb->cam_target_lun );
            break;

        case xpt_scan_res_unchanged:
            /*      if( temp_device )
                        xpt_remove_device( ccb->xpt_bus, ccb->cam_target_id, ccb->cam_target_lun );
            */
            break;

        case xpt_scan_res_new:
            memcpy( &device->inquiry_data,
                    &new_inquiry_data, sizeof( new_inquiry_data ));

            device->temporary = false;

            xpt_call_async_all( device->bus, device->target_id,
                                device->target_lun, AC_FOUND_DEVICE, NULL, 0, true );
            break;

        case xpt_scan_res_replaced: {
            xpt_device_info *new_device;

            xpt_call_async_all( device->bus, device->target_id,
                                device->target_lun, AC_LOST_DEVICE, NULL, 0, true );

            /* !issue moved to xpt_action!
            // if replaced, we could block previous device handles, but
            // this is dangerous - the device may get replaced at some time,
            // access to the new device via the old device handled established
            // and later on scan_lun invoked; thus, the old device handle is
            // actually solely a connection to the new device, so blocking it
            // would break it
            */
            device->temporary = true;

            //xpt_remove_device( ccb->xpt_bus, ccb->cam_target_id, ccb->cam_target_lun );
            new_device = xpt_get_device( device->bus, device->target_id, device->target_lun );

            if ( new_device ) {
                new_device->temporary = false;
                memcpy( &new_device->inquiry_data,
                        &new_inquiry_data, sizeof( new_inquiry_data ));

                xpt_call_async_all( device->bus, device->target_id,
                                    device->target_lun, AC_FOUND_DEVICE, NULL, 0, true );
            }
            break;
        }
        default:
            panic( "SCSI BUG: invalid result of scan lun\n" );
    }

    xpt_free_ccb( &worker_req->cam_ch );

    ccb->cam_status = CAM_REQ_CMP;

err:
    sem_release( ccb->xpt_bus->scan_lun_lock, 1 );

    xpt_done( ccb );
}

/*void xpt_scan_lun( CCB_HEADER *ccb )
{
    xpt_scan_lun_int( ccb );
    xpt_done( ccb );
}
*/

int xpt_rescan_threadproc( void *arg )
{
    SHOW_FLOW0( 3, "" );

    while ( 1 ) {
        CCB_HEADER *ccb;
        int path_id;

        sem_acquire( xpt_start_rescan, 1 );

        SHOW_FLOW0( 3, "1" );

        if ( xpt_shutting_down )
            break;

        for ( path_id = 0; path_id <= xpt_highest_path_id; ++path_id ) {
            xpt_bus_cookie bus;

            bus = xpt_get_bus( path_id );

            SHOW_FLOW0( 3, "2" );

            if ( bus ) {
                if ( bus->do_rescan ) {
                    xpt_device_cookie device;

                    SHOW_FLOW0( 3, "3" );

                    device = xpt_get_device( bus, XPT_BUS_GLOBAL_ID, XPT_BUS_GLOBAL_ID );
                    if ( device ) {

                        ccb = xpt_alloc_ccb( device );
                        if ( ccb != NULL ) {
                            SHOW_FLOW( 3, "path_id=%i", path_id );

                            ccb->cam_func_code = XPT_SCAN_BUS;
                            //ccb->cam_path_id = path_id;
                            ccb->cam_flags = 0;
                            xpt_action( ccb );

                            bus->do_rescan = false;

                            SHOW_FLOW0( 3, "4" );

                            xpt_free_ccb( ccb );
                        }
                        xpt_put_device( device );
                    }
                }

                xpt_put_bus( bus );
            }
        }

        SHOW_FLOW0( 3, "5" );
    }

    return 1;
}

void xpt_schedule_busscan( xpt_bus_info *bus )
{
    SHOW_FLOW( 3, "bus=%p", bus );
    bus->do_rescan = true;
    sem_release( xpt_start_rescan, 1 );
}

void xpt_scan_bus( CCB_HEADER *ccb )
{
    xpt_bus_info *bus = ccb->xpt_bus;
    CCB_HEADER *query_ccb = NULL;
    CCB_PATHINQ *path_inq_ccb;
    int initiator_id, target;
    xpt_device_cookie global_device;

    SHOW_FLOW0( 3, "" );

    global_device = xpt_get_device( ccb->xpt_bus, XPT_BUS_GLOBAL_ID, XPT_BUS_GLOBAL_ID );
    if ( global_device == NULL ) {
        ccb->cam_status = CAM_PATH_INVALID;
        goto err;
    }

    query_ccb = xpt_alloc_ccb( global_device );

    if ( query_ccb == NULL ) {
        // XXX what is the proper return code for "out of memory"?
        ccb->cam_status = CAM_BUSY;
        goto err1;
    }

    SHOW_FLOW0( 3, "1" );

    query_ccb->cam_func_code = XPT_PATH_INQ;
    //query_ccb->cam_path_id = ccb->cam_path_id;
    query_ccb->cam_flags = 0;

    xpt_action( query_ccb );

    if ( query_ccb->cam_status != CAM_REQ_CMP ) {
        ccb->cam_status = query_ccb->cam_status;
        goto err2;
    }

    path_inq_ccb = (CCB_PATHINQ *)query_ccb;

    initiator_id = path_inq_ccb->cam_initiator_id;

    SHOW_FLOW( 3, "initiator_id=%i", initiator_id );

    // tell SIM to rescan bus (needed at least by IDE translator)
    // as this function is optional for SIM, we ignore its result
    query_ccb->cam_func_code = XPT_SCAN_BUS;
    //query_ccb->cam_path_id = ccb->cam_path_id;
    query_ccb->cam_flags = 0;

    query_ccb->xpt_bus = bus;
    query_ccb->xpt_state = XPT_STATE_SENT;
    bus->interface->action( bus->sim_cookie, query_ccb );

    xpt_free_ccb( query_ccb );

    for ( target = 0; target <= 1/*MAX_TARGET_ID*/; ++target ) {
        int lun;

        SHOW_FLOW( 3, "target: %i", target );

        if ( target == initiator_id )
            continue;

        //query_ccb->cam_target_id = target;

        for ( lun = 0; lun <= 1/*MAX_LUN_ID*/; ++lun ) {
            xpt_device_cookie device;

            SHOW_FLOW( 3, "lun: %i", lun );

            device = xpt_get_device( bus, target, lun );
            if ( device == NULL )
                break;

            SHOW_FLOW0( 3, "got device" );

            query_ccb = xpt_alloc_ccb( device );
            if ( query_ccb == NULL ) {
                xpt_put_device( device );
                break;
            }

            SHOW_FLOW0( 3, "got query_ccb" );

            query_ccb->cam_func_code = XPT_SCAN_LUN;
            //query_ccb->cam_target_lun = lun;

            xpt_action( query_ccb );

            SHOW_FLOW0( 3, "query finished" );

            xpt_free_ccb( query_ccb );

            xpt_put_device( device );

            if ( lun == 0 && query_ccb->cam_status != CAM_REQ_CMP )
                break;
        }
    }

    SHOW_FLOW0( 3, "done" );

    ccb->cam_status = CAM_REQ_CMP;
    xpt_done( ccb );
    return;

err2:
    xpt_free_ccb( query_ccb );
err1:
    xpt_put_device( global_device );
    // we unlock it ourself
    /*ref_lock_nonexcl_unlock( &ccb->xpt_bus->lock );*/
err:
    xpt_done( ccb );

    return;
}

void xpt_get_device_type( CCB_HEADER *ccb )
{
    CCB_GETDEV *request = (CCB_GETDEV *)ccb;
    xpt_device_info *device = ccb->xpt_device;

    if ( device->temporary ) {
        ccb->cam_status = CAM_DEV_NOT_THERE;
    } else {
        request->cam_pd_type = device->inquiry_data.device_type;

        if ( request->cam_inq_data )
            memcpy( request->cam_inq_data, &device->inquiry_data, INQLEN );

        ccb->cam_status = CAM_REQ_CMP;
    }

    xpt_done( ccb );
}
