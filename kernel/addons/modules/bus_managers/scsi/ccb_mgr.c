/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include "xpt_internal.h"
#include "ccb_mgr.h"
#include <kernel/vm.h>

//locked_pool_cookie ccb_pool;

// ccb are relatively large, so don't make it too small to not waste memory
#define CCB_CHUNK_SIZE 16*1024

// maximum number of CCBs - probably, we want to make that editable
#define CCB_NUM_MAX 128

CCB_HEADER *xpt_alloc_ccb( xpt_device_info *device )
{
    CCB_HEADER *ccb;

    SHOW_FLOW0( 3, "" );

    ccb = (CCB_HEADER *)locked_pool->alloc( device->bus->ccb_pool );
    ccb->xpt_state = XPT_STATE_FINISHED;
    ccb->xpt_device = device;
    ccb->cam_target_id = device->target_id;
    ccb->cam_target_lun = device->target_lun;
    SHOW_FLOW( 3, "path=%i", ccb->cam_path_id );

    return ccb;
}

void xpt_free_ccb( CCB_HEADER *ccb )
{
    SHOW_FLOW0( 3, "" );

    if ( ccb->xpt_state != XPT_STATE_FINISHED ) {
        panic( "Tried to free ccb that's still in use (state %i)\n",
               ccb->xpt_state );
    }

    ccb->xpt_state = XPT_STATE_FREE;

    locked_pool->free( ccb->xpt_bus->ccb_pool, ccb );
}

static int ccb_low_alloc_hook( void *block, void *arg )
{
    CCB_HEADER *ccb = (CCB_HEADER *)block;
    xpt_bus_info *bus = (xpt_bus_info *)arg;
    int res;

    vm_get_page_mapping( vm_get_kernel_aspace_id(), (addr_t)ccb,
                         (addr_t *)&ccb->phys_addr );
    ccb->xpt_bus = bus;
    ccb->cam_path_id = bus->path_id;
    ccb->xpt_state = XPT_STATE_FREE;

    if ((res = ccb->completion_sem = sem_create( 0, "ccb_sem" )) < 0 )
        return res;

    return NO_ERROR;
}

static void ccb_low_free_hook( void *block, void *arg )
{
    CCB_HEADER *ccb = (CCB_HEADER *)block;

    sem_delete( ccb->completion_sem );
}

int xpt_init_ccb_alloc( xpt_bus_info *bus )
{
    // initially, we want no CCB allocated as the path_id of
    // the bus is not ready yet so the CCB cannot be initialized
    // correctly
    bus->ccb_pool = locked_pool->init( sizeof( CCB_SIZE_UNION ),
                                       sizeof( uint32 ) - 1, offsetof( CCB_SIZE_UNION, csio.cam_ch ),
                                       CCB_CHUNK_SIZE, CCB_NUM_MAX, 0,
                                       "xpt_ccb_pool", REGION_WIRING_WIRED_CONTIG,
                                       ccb_low_alloc_hook, ccb_low_free_hook, bus );

    if ( bus->ccb_pool == NULL )
        return ERR_NO_MEMORY;

    return NO_ERROR;
}

void xpt_uninit_ccb_alloc( xpt_bus_info *bus )
{
    locked_pool->uninit( bus->ccb_pool );
}
