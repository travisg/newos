/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "scsi_periph_int.h"

#include <kernel/module.h>

int scsi_periph_init( void );
int scsi_periph_uninit( void );

int scsi_periph_init( void )
{
    int res;

    SHOW_FLOW0( 3, "" );

    if ( (res = module_get( CAM_FOR_DRIVER_MODULE_NAME, 0, (void **)&xpt )) != NO_ERROR )
        return res;

    return NO_ERROR;
}

int scsi_periph_uninit( void )
{
    SHOW_FLOW0( 3, "" );

    module_put( CAM_FOR_DRIVER_MODULE_NAME );
    return NO_ERROR;
}

scsi_periph_interface driver_interface = {
    media_changed,
    scsi_check_capacity,
    check_error,
    send_start_stop,
    scsi_get_media_status,
    compose_device_name,
    handle_open,
    handle_close,

    scsi_register_driver,
    scsi_unregister_driver
};


struct module_header scsi_periph_module = {
    SCSI_PERIPH_MODULE_NAME,
    MODULE_CURR_VERSION,
    0,
    &driver_interface,

    scsi_periph_init,
    scsi_periph_uninit
};

struct module_header *modules[] = {
    &scsi_periph_module, NULL
};
