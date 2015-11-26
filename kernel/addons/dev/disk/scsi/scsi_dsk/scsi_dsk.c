/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/arch/cpu.h>
#include <kernel/heap.h>
#include <kernel/dev/blkman.h>
#include <kernel/module.h>
#include <kernel/bus/scsi/scsi_periph.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define debug_level_flow 3
#define debug_level_error 3
#define debug_level_info 3

#define DEBUG_MSG_PREFIX "SCSI_DSK -- "

#include <kernel/debug_ext.h>

#define DAS_STD_TIMEOUT 10000000

#define SCSI_DSK_MODULE_NAME "dev/disk/scsi/scsi_dsk"


extern blkdev_interface das_interface;

blkman_interface *blkman;
cam_for_driver_interface *xpt;
scsi_periph_interface *scsi_periph;

scsi_driver_cookie scsi_periph_cookie;

typedef struct das_device_info {
    scsi_device_cookie cookie;
    xpt_device_cookie xpt_device;
    int rw10_enabled;       // access must be atomic
    blkdev_params params;
    int next_tag_action;
    bool removable;
    char *name;
    blkman_dev_cookie blkman_cookie;
} das_device_info;

typedef struct das_handle_info {
    scsi_handle_cookie cookie;
    das_device_info *device;
    int pending_error;
} das_handle_info;

int das_read_write( das_handle_info *handle, const phys_vecs *vecs,
                    off_t pos, size_t num_blocks, size_t *bytes_transferred, bool write );

static int das_read( das_handle_info *handle, const phys_vecs *vecs,
                     off_t pos, size_t num_blocks, size_t *bytes_transferred )
{
    return das_read_write( handle, vecs, pos,
                           num_blocks, bytes_transferred, false );
}

static int das_write( das_handle_info *handle, const phys_vecs *vecs,
                      off_t pos, size_t num_blocks, size_t *bytes_transferred )
{
    return das_read_write( handle, vecs, pos,
                           num_blocks, bytes_transferred, true );
}

int das_read_write( das_handle_info *handle, const phys_vecs *vecs,
                    off_t pos, size_t num_blocks, size_t *bytes_transferred, bool write )
{
    das_device_info *device = handle->device;
    CCB_SCSIIO *request;
    err_res res;
    int retries = 0;
    int err;

    if ( num_blocks >= 0x10000 ) {
        num_blocks = 0x10000;
    }

    if ( pos >= 0x100000000 )
        return ERR_INVALID_ARGS;

    request = (CCB_SCSIIO *)xpt->ccb_alloc( device->xpt_device );

    if ( request == NULL )
        return ERR_NO_MEMORY;

    do {
        CCB_HEADER *ccb;
        int num_bytes;

        ccb = &request->cam_ch;

        ccb->cam_func_code = XPT_SCSI_IO;
        //ccb->cam_path_id = device->path_id;
        //ccb->cam_target_id = device->target_id;
        //ccb->cam_target_lun = device->target_lun;
        ccb->cam_flags = write ? CAM_DIR_OUT : CAM_DIR_IN;

        if ( !device->rw10_enabled && num_blocks > 0x100 )
            num_blocks = 0x100;

        num_bytes = num_blocks * device->params.block_size;

        request->cam_data = NULL;
        request->cam_sg_list = (struct sg_elem *)vecs->vec;
        request->cam_dxfer_len = num_bytes;
        request->cam_sglist_cnt = vecs->num;
        request->cam_sort = pos;
        request->cam_timeout = DAS_STD_TIMEOUT;
        request->cam_tag_action = atomic_set( &device->next_tag_action,
                                              CAM_SIMPLE_QTAG );

        if ( pos + num_blocks < 0x200000 && num_blocks <= 0x100 ) {
            scsi_cmd_rw_6 *cmd = (scsi_cmd_rw_6 *)request->cam_cdb;

            memset( cmd, 0, sizeof( *cmd ));
            cmd->opcode = write ? SCSI_OP_WRITE_6 : SCSI_OP_READ_6;
            cmd->high_LBA = (pos >> 16) & 0x1f;
            cmd->mid_LBA = (pos >> 8) & 0xff;
            cmd->low_LBA = pos & 0xff;
            cmd->length = num_blocks;

            request->cam_cdb_len = sizeof( *cmd );
        } else {
            scsi_cmd_rw_10 *cmd = (scsi_cmd_rw_10 *)request->cam_cdb;

            if ( !device->rw10_enabled ) {
                err = ERR_INVALID_ARGS;
                goto abort;
            }

            memset( cmd, 0, sizeof( *cmd ));
            cmd->opcode = write ? SCSI_OP_WRITE_10 : SCSI_OP_READ_10;
            cmd->RelAdr = 0;
            cmd->FUA = 0;
            cmd->DPO = 0;

            cmd->top_LBA = (pos >> 24) & 0xff;
            cmd->high_LBA = (pos >> 16) & 0xff;
            cmd->mid_LBA = (pos >> 8) & 0xff;
            cmd->low_LBA = pos & 0xff;

            cmd->high_length = (num_blocks >> 8) & 0xff;
            cmd->low_length = num_blocks & 0xff;

            request->cam_cdb_len = sizeof( *cmd );
        }

        err = handle->pending_error;

        if ( err )
            goto abort;

        xpt->action( &request->cam_ch );

        sem_acquire( request->cam_ch.completion_sem, 1 );

        res = scsi_periph->check_error( device->cookie, request );

        switch ( res.action ) {
            case err_act_ok:
                *bytes_transferred = num_bytes - request->cam_resid;
                break;

            case err_act_start:
                res = scsi_periph->send_start_stop(
                          device->cookie, request, 1, device->removable );
                if ( res.action == err_act_ok )
                    res.action = err_act_retry;
                break;

            case err_act_invalid_req:
                if ( device->rw10_enabled ) {
                    device->rw10_enabled = false;
                    res.action = err_act_retry;
                } else
                    res.action = err_act_fail;
                break;
        }

    } while ( res.action == err_act_retry && retries++ < 3 );

    xpt->ccb_free( (CCB_HEADER *)request );

    if ( res.error_code == ERR_DEV_READ_ERROR && write )
        return ERR_DEV_WRITE_ERROR;
    else
        return res.error_code;

abort:
    xpt->ccb_free( (CCB_HEADER *)request );

    return err;
}


// unused
#if 0
static int log2( uint32 x )
{
    int y;

    for ( y = 31; y >= 0; --y )
        if ( x == ((uint32)1 << y) )
            break;

    return y;
}
#endif

static int das_ioctl( das_handle_info *handle, int op, void *buf, size_t len )
{
    switch ( op ) {
        case IOCTL_GET_MEDIA_STATUS:
            return scsi_periph->get_media_status( handle->cookie );

        default:
            return ERR_INVALID_ARGS;
    }
}

static int das_device_init( periph_cookie periph, scsi_device_cookie cookie,
                            scsi_res_inquiry *device_inquiry, CAM_SIM_PARAMS *sim_inquiry,
                            xpt_device_cookie xpt_device, CCB_HEADER *ccb, das_device_info **device_out )
{
    das_device_info *device;
    int res;

    SHOW_FLOW0( 3, "" );

    device = kmalloc( sizeof( *device ));
    if ( device == NULL )
        return ERR_NO_MEMORY;

    memset( device, 0, sizeof( *device ));
    device->cookie = cookie;
    device->xpt_device = xpt_device;
    device->removable = device_inquiry->RMB;

    device->next_tag_action = CAM_SIMPLE_QTAG;
    device->rw10_enabled = true;

    memset( &device->params, 0, sizeof( device->params ));
    device->params.alignment = sim_inquiry->dma_alignment;
    device->params.max_sg_num = max( sim_inquiry->max_sg_num, 1 );
    device->params.max_blocks = min( sim_inquiry->max_blocks, 0x10000 );

    device->name = scsi_periph->compose_device_name( device->cookie, "disk/scsi" );
    if ( device->name == NULL ) {
        res = ERR_NO_MEMORY;
        goto err;
    }

    SHOW_FLOW( 3, "name=%s", device->name );

    device->params.alignment = sim_inquiry->dma_alignment;
    device->params.max_blocks = sim_inquiry->max_blocks;
    device->params.dma_boundary = sim_inquiry->dma_boundary;
    device->params.dma_boundary_solid = sim_inquiry->dma_boundary_solid;
    device->params.max_sg_num = sim_inquiry->max_sg_num;

    // return device pointer before using scsi_periph calls
    *device_out = device;

    SHOW_FLOW0( 3, "2" );

    scsi_periph->check_capacity( device->cookie, ccb );

    SHOW_FLOW0( 3, "3" );

    res = blkman->register_blkdev( &das_interface, (blkdev_cookie)device,
                                   device->name, &device->blkman_cookie, &device->params );
    if ( res != NO_ERROR )
        goto err2;

    SHOW_FLOW0( 3, "done" );
    return NO_ERROR;

err2:
    kfree( device->name );
err:
    kfree( device );
    return res;
}

static bool das_is_supported( periph_cookie periph, CCB_GETDEV *dev_type )
{
    return dev_type->cam_pd_type == scsi_dev_direct_access;
}


static int das_device_removed( das_device_info *device )
{
    blkman->unregister_blkdev( device->blkman_cookie );
    return NO_ERROR;
}

static void das_handle_set_error( das_handle_info *handle, int error_code )
{
    handle->pending_error = error_code;
}

static int das_handle_get_error( das_handle_info *handle )
{
    return handle->pending_error;
}

static void das_async( periph_cookie periph,
                       int path_id, int target_id, int lun,
                       int code, const void *data, int data_len )
{
}

static int das_device_uninit( das_device_info *device )
{
    kfree( device->name );
    kfree( device );
    return NO_ERROR;
}

static void das_set_capacity( das_device_info *device, uint64 capacity,
                              uint32 block_size )
{
    SHOW_FLOW( 3, "device=%p", device );

    device->params.capacity = capacity;
    device->params.block_size = block_size;

    // during init, capacity is checked before device is registered
    // at blkman, thus we need this test
    if ( device->blkman_cookie != NULL )
        blkman->set_capacity( device->blkman_cookie, capacity, block_size );
}

scsi_periph_callbacks callbacks = {
    (bool (*)( periph_cookie, CCB_GETDEV * ))       das_is_supported,
    (int (*)( periph_cookie, scsi_device_cookie,
    scsi_res_inquiry *, CAM_SIM_PARAMS *,
    xpt_device_cookie, CCB_HEADER *,
    periph_device_cookie * ))                   das_device_init,
    (int (*)( periph_device_cookie ))               das_device_removed,
    (int (*)( periph_device_cookie periph_device )) das_device_uninit,
    (void (*)( periph_handle_cookie, int ))         das_handle_set_error,
    (int (*)( periph_handle_cookie ))               das_handle_get_error,
    (void (*)( periph_device_cookie,
    uint64, uint32 ))                           das_set_capacity,

    (void (*)( periph_cookie,
    int, int, int, int, const void *, int ))    das_async
};


static int das_open( das_device_info *device, das_handle_info **handle_out )
{
    das_handle_info *handle;
    int res;

    handle = kmalloc( sizeof( *handle ));
    if ( handle == NULL )
        return ERR_NO_MEMORY;

    handle->device = device;

    res = scsi_periph->handle_open( device->cookie, (periph_handle_cookie)handle,
                                    &handle->cookie );
    if ( res != NO_ERROR ) {
        kfree( handle );
        return res;
    }

    *handle_out = handle;
    return NO_ERROR;
}

static int das_close( das_handle_info *handle )
{
    scsi_periph->handle_close( handle->cookie );
    kfree( handle );

    return NO_ERROR;
}


blkdev_interface das_interface = {
    (int (*)( blkdev_cookie, blkdev_handle_cookie * ))          &das_open,
    (int (*)( blkdev_handle_cookie ))                           &das_close,

    (int (*)( blkdev_handle_cookie, const phys_vecs *,
    off_t, size_t, size_t * ))                              &das_read,
    (int (*)( blkdev_handle_cookie, const phys_vecs *,
    off_t, size_t, size_t * ))                              &das_write,

    (int (*)( blkdev_handle_cookie, int, void *, size_t ))      &das_ioctl,
};


static int das_init( void )
{
    int res;

    SHOW_FLOW0( 3, "" );

    res = module_get( CAM_FOR_DRIVER_MODULE_NAME, 0, (void **)&xpt );
    if ( res != NO_ERROR )
        return res;

    SHOW_FLOW0( 3, "1" );

    res = module_get( BLKMAN_MODULE_NAME, 0, (void **)&blkman );
    if ( res != NO_ERROR )
        goto err1;

    SHOW_FLOW0( 3, "2" );

    res = module_get( SCSI_PERIPH_MODULE_NAME, 0, (void **)&scsi_periph );
    if ( res != NO_ERROR )
        goto err2;

    SHOW_FLOW0( 3, "3" );

    res = scsi_periph->register_driver( &callbacks,
                                        NULL, &scsi_periph_cookie );
    if ( res != NO_ERROR )
        goto err3;

    SHOW_FLOW0( 3, "4" );

    return NO_ERROR;

err3:
    module_put( SCSI_PERIPH_MODULE_NAME );
err2:
    module_put( BLKMAN_MODULE_NAME );
err1:
    module_put( CAM_FOR_DRIVER_MODULE_NAME );
    return res;
}

static int das_uninit( void )
{
    int res;

    SHOW_FLOW0( 3, "" );

    res = scsi_periph->unregister_driver( scsi_periph_cookie );
    if ( res != NO_ERROR )
        return res;

    module_put( SCSI_PERIPH_MODULE_NAME );
    module_put( BLKMAN_MODULE_NAME );
    module_put( CAM_FOR_DRIVER_MODULE_NAME );

    return NO_ERROR;
}

// dummy to make module manager happy
void *das_module_interface = NULL;

module_header scsi_dsk_module = {
    SCSI_DSK_MODULE_NAME,
    MODULE_CURR_VERSION,
    0,

    &das_module_interface,

    das_init,
    das_uninit
};

module_header *modules[] = {
    &scsi_dsk_module,
    NULL
};
