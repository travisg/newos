/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include "xpt_internal.h"

#include <kernel/module.h>
#include <kernel/debug.h>

#include "device_mgr.h"
#include "bus_mgr.h"
#include "periph_mgr.h"
#include "ccb_mgr.h"
#include "sg_mgr.h"

// scanning SCSI busses is time consuming - give it normal priority
#define SCAN_BUSSES_PRIORITY THREAD_MEDIUM_PRIORITY //5

#define CAM_XPT_MODULE_NAME "bus_managers/scsi/xpt/v1"


// these could be moved to appropriate header but would lead
// to inclusion of kernel/module.h in them
extern module_header xpt_driver_module, xpt_sim_module;

locked_pool_interface *locked_pool;

thread_id xpt_rescan_thread;
bool xpt_shutting_down;

// to add or remove a bus, grab this lock
recursive_lock registration_lock;

static int xpt_init()
{
    int res;
    int tmp;

    SHOW_FLOW0( 3, "" );

    if ( (res = module_get( LOCKED_POOL_MODULE_NAME, 0, (void **)&locked_pool)) < 0 )
        goto err;

    if ( (res = recursive_lock_create( &registration_lock )) < 0 )
        goto err1;

    xpt_highest_path_id = 0;
    xpt_periphs = NULL;
    xpt_shutting_down = false;

    if ( (res = mutex_init( &xpt_periphs_mutex, "xpt_periphs_mutex" )) < 0 )
        goto err2;

    if ( (res = xpt_start_rescan = sem_create( 0, "xpt_start_rescan" )) < 0 ) {
        res = xpt_start_rescan;
        goto err4;
    }

    if ( (res = xpt_rescan_thread = thread_create_kernel_thread( "xpt_rescan_busses",
                                    (int (*)())xpt_rescan_threadproc, NULL )) < 0 ) {
        res = xpt_rescan_thread;
        goto err5;
    }

    thread_set_priority( xpt_rescan_thread, SCAN_BUSSES_PRIORITY );
    thread_resume_thread( xpt_rescan_thread );

    /*if( (res = xpt_init_ccb_alloc()) < 0 )
        goto err6;*/

    if ( (res = init_temp_sg()) < 0 )
        goto err7;

    //load_periph_drivers();

    return NO_ERROR;

err7:
    /*xpt_uninit_ccb_alloc();*/
//err6:
    xpt_shutting_down = true;
    sem_release( xpt_start_rescan, 1 );
    thread_wait_on_thread( xpt_rescan_thread, &tmp );
err5:
    sem_delete( xpt_start_rescan );
err4:
    mutex_destroy( &xpt_periphs_mutex );
err2:
    recursive_lock_destroy( &registration_lock );
err1:
    module_put( LOCKED_POOL_MODULE_NAME );
err:
    return res;
}

static int xpt_uninit()
{
    int tmp;

    SHOW_INFO0( 3, "" );

    xpt_shutting_down = true;

    sem_release( xpt_start_rescan, 1 );
    thread_wait_on_thread( xpt_rescan_thread, &tmp );

    sem_delete( xpt_start_rescan );
    recursive_lock_destroy( &registration_lock );
    mutex_destroy( &xpt_periphs_mutex );

    /*xpt_uninit_ccb_alloc();*/
    uninit_temp_sg();

    module_put( LOCKED_POOL_MODULE_NAME );

    return NO_ERROR;
}

int xpt_driver_sim_init()
{
    int res;
    void *tmp;

    SHOW_FLOW0( 3, "" );

    res = module_get( CAM_XPT_MODULE_NAME, 0, &tmp );

    if ( res < 0 ) {
        SHOW_ERROR0( 0, "Cannot lock XPT module" );
        return res;
    }

    return NO_ERROR;
}

int xpt_driver_sim_uninit()
{
    SHOW_FLOW0( 3, "" );
    return module_put( CAM_XPT_MODULE_NAME );
}


// add an empty entry to make module handler happy
int xpt_interface[] = { 0 };

module_header xpt_module = {
    CAM_XPT_MODULE_NAME,
    MODULE_CURR_VERSION,
    0,
    &xpt_interface,

    xpt_init,
    xpt_uninit
};

module_header *modules[] = {
    &xpt_driver_module,
    &xpt_sim_module,
    &xpt_module,
    NULL
};
