/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"
#include "periph_mgr.h"

#include <kernel/heap.h>
#include <kernel/module.h>
#include <kernel/debug.h>

#include "bus_mgr.h"

mutex xpt_periphs_mutex;
xpt_periph_info *xpt_periphs;

static void inform_driver_about_devices( xpt_periph_info *periph )
{
    int path_id;

    SHOW_FLOW0( 3, "" );

    recursive_lock_lock( &registration_lock );

    for ( path_id = 0; path_id < MAX_PATH_ID; ++path_id ) {
        int target_id;
        xpt_bus_info *bus;

        bus = xpt_busses[path_id];
        if ( bus == NULL )
            continue;

        for ( target_id = 0; target_id <= MAX_TARGET_ID; ++target_id ) {
            int target_lun;

            for ( target_lun = 0; target_lun <= MAX_LUN_ID; ++target_lun ) {
                xpt_target_info *target;
                xpt_device_info *device;

                // check target each time - not sure whether recursive
                // registration lock really requiers that though...
                target = bus->targets[target_id];
                if ( target == NULL )
                    break;

                device = target->devices[target_lun];
                if ( device == NULL )
                    continue;

                if ( device->temporary )
                    continue;

                periph->interface->async( periph->periph_cookie,
                                          path_id, target_id, target_lun,
                                          AC_FOUND_DEVICE, NULL, 0 );
            }
        }
    }

    recursive_lock_unlock( &registration_lock );
}

xpt_periph_info *xpt_register_driver( cam_periph_interface *interface,
                                      cam_periph_cookie periph_cookie )
{
    xpt_periph_info *periph;

    SHOW_FLOW0( 3, "" );

    periph = kmalloc( sizeof( *periph ));
    if ( periph == NULL )
        return NULL;

    mutex_lock( &xpt_periphs_mutex );

    if ( xpt_periphs )
        xpt_periphs->prev = periph;

    periph->next = xpt_periphs;
    periph->prev = NULL;
    xpt_periphs = periph;

    periph->interface = interface;
    periph->periph_cookie = periph_cookie;

    mutex_unlock( &xpt_periphs_mutex );

    inform_driver_about_devices( periph );

    return periph;
}

int xpt_unregister_driver( xpt_periph_info *periph )
{
    mutex_lock( &xpt_periphs_mutex );

    if ( periph->next )
        periph->next->prev = periph->prev;

    if ( periph->prev )
        periph->prev->next = periph->next;
    else
        xpt_periphs = periph->next;

    mutex_unlock( &xpt_periphs_mutex );

    kfree( periph );

    return NO_ERROR;
}

