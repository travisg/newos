/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "ide_internal.h"

#include <kernel/module.h>
#include <kernel/debug.h>

#include "channel_mgr.h"
#include "ide_sim.h"
#include "sync.h"

#define MAX_PIO_RETRIES 2


static int ide_bus_manager_init()
{
	int res;
	
	SHOW_FLOW0( 3, "\n" );
	
	res = module_get( CAM_FOR_SIM_MODULE_NAME, 0, (void **)&xpt );
	
	if( res < 0 ) {
		SHOW_ERROR0( 0, "Cannot connect to XPT module\n" );
		return res;
	}
	
	return NO_ERROR;
}

static int ide_bus_manager_uninit()
{
	return module_put( CAM_FOR_SIM_MODULE_NAME );
}


ide_bus_manager_interface bus_manager_interface = {
	irq_handler,
	connect_channel,
	disconnect_channel
};

module_header ide_bus_manager_module = {
	IDE_BUS_MANAGER_NAME,
	MODULE_CURR_VERSION,
	0,
	&bus_manager_interface,
	
	ide_bus_manager_init,
	ide_bus_manager_uninit
};

module_header *modules[] =
{
	&ide_bus_manager_module,
	NULL
};
