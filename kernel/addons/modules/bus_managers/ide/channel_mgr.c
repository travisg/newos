/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "ide_internal.h"
#include "channel_mgr.h"

#include <kernel/heap.h>
#include <string.h>
#include <kernel/module.h>

#include "sync.h"
#include "device_mgr.h"
#include "ide_sim.h"

static void disconnect_worker( ide_bus_info *bus, void *arg )
{
	int i;
	
	for( i = 0; i < bus->controller_params.max_devices; ++i ) {
		if( bus->devices[i] )
			// is this the proper error code?
			finish_all_requests( bus->devices[i], CAM_NO_HBA );
	}
	
	// don't call stuff like unregister_SIM here
	// as we are in service thread content
	xpt->unblock_bus( bus->xpt_cookie );
}


static void call_xpt_bus_scan( ide_bus_info *bus )
{
	cam_for_driver_interface *xpt_driver;
	CCB_HEADER *ccb;
	xpt_bus_cookie bus_cookie;
	xpt_device_cookie device_cookie;
	
	if( module_get( CAM_FOR_DRIVER_MODULE_NAME, 0, (void **)&xpt_driver ) < 0 )
		return;

	bus_cookie = xpt_driver->get_bus( bus->path_id );
	if( bus_cookie == NULL )
		goto err;
		
	device_cookie = xpt_driver->get_device( bus_cookie, XPT_BUS_GLOBAL_ID, XPT_BUS_GLOBAL_ID );
	if( device_cookie == NULL )
		goto err1;
		
	ccb = xpt_driver->ccb_alloc( device_cookie );
	if( ccb == NULL )
		goto err2;
		
	ccb->cam_func_code = XPT_SCAN_BUS;
	//ccb->cam_path_id = bus->path_id;
	ccb->cam_flags = 0;
	
	xpt_driver->action( ccb );
	
	xpt_driver->ccb_free( ccb );
err2:
	xpt_driver->put_device( device_cookie );
err1:
	xpt_driver->put_bus( bus_cookie );
err:
	module_put( CAM_FOR_DRIVER_MODULE_NAME );
	return;
}

int connect_channel( const char *name, 
	ide_controller_interface *controller,
	ide_channel_cookie channel,
	ide_controller_params *controller_params,
	ide_bus_info **bus_out )
{
	ide_bus_info *bus;
	xpt_bus_cookie xpt_cookie;
	int res;
	
	SHOW_FLOW0( 3, "" );
	
	bus = kmalloc( sizeof( *bus ));
	if( bus == NULL )
		return ERR_NO_MEMORY;
		
	SHOW_FLOW0( 3, "1" );
		
	memset( bus, 0, sizeof( *bus ));
	memcpy( &bus->controller_params, controller_params, sizeof( *controller_params ));
	bus->lock = 0;
	bus->num_running_reqs = 0;
	bus->controller = controller;
	bus->channel = channel;
	bus->active_qrequest = NULL;
	bus->disconnected = false;
	
	init_synced_pc( &bus->scan_bus_syncinfo, scan_device_worker );
	init_synced_pc( &bus->disconnect_syncinfo, disconnect_worker );
	
	bus->wait_id = bus->dpc_id = 0;
	bus->xpt_cookie = NULL;
	timer_setup_timer( ide_timeout, bus, &bus->timer );
	
	bus->state = ide_state_idle;
	bus->device_to_reconnect = NULL;
	
	bus->synced_pc_list = NULL;
	if( (res = xpt->alloc_dpc( &bus->irq_dpc )) < 0 )
		goto err1;
		
	SHOW_FLOW0( 3, "2" );
		
	bus->active_device = NULL;
	bus->sync_wait_sem = sem_create( 0, "ide_sync_wait" );
	if( bus->sync_wait_sem < 0 ) {
		res = bus->sync_wait_sem;
		goto err2;
	}
		
	SHOW_FLOW0( 3, "3" );
		
	bus->devices[0] = bus->devices[1] = NULL;
	
	bus->scan_device_sem = sem_create( 0, "ide_scan_finished" );
	if( bus->scan_device_sem < 0 ) {
		res = bus->scan_device_sem;
		goto err3;
	}
		
	SHOW_FLOW0( 3, "4" );
		
	bus->first_device = NULL;
	strncpy( bus->controller_name, name, HBA_ID );
	
	res = xpt->register_SIM( &ide_sim_interface, (cam_sim_cookie) bus, 
		&xpt_cookie );
		
	if( res < 0 )
		goto err4;
		
	SHOW_FLOW0( 3, "5" );
		
	bus->path_id = res;
	// do assignment very last as the irq handler uses this value, so it
	// must be either NULL or valid
	bus->xpt_cookie = xpt_cookie;
	
	// after this, the bus must be fully functional
	*bus_out = bus;

	call_xpt_bus_scan( bus );
		
	return NO_ERROR;
	
err4:
	sem_delete( bus->scan_device_sem );
	
err3:
	sem_delete( bus->sync_wait_sem );

err2:	
	xpt->free_dpc( bus->irq_dpc );

err1:	
	uninit_synced_pc( &bus->scan_bus_syncinfo );
	uninit_synced_pc( &bus->disconnect_syncinfo );
	kfree( bus );
	
	return res;
}

void sim_unregistered( ide_bus_info *bus )
{
	sem_delete( bus->scan_device_sem );
	sem_delete( bus->sync_wait_sem );
	xpt->free_dpc( bus->irq_dpc );
	uninit_synced_pc( &bus->scan_bus_syncinfo );
	uninit_synced_pc( &bus->disconnect_syncinfo );
	
	kfree( bus );
}


int disconnect_channel( ide_bus_info *bus )
{
	xpt->block_bus( bus->xpt_cookie );
	bus->disconnected = true;
	schedule_synced_pc( bus, &bus->disconnect_syncinfo, NULL );
	
	xpt->unregister_SIM( bus->path_id );
		
	return NO_ERROR;
}	
