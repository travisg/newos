/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"
#include "bus_mgr.h"

#include <kernel/heap.h>
#include <nulibc/string.h>
#include <kernel/module.h>

#include "dpc.h"
#include "xpt_io.h"
#include "device_mgr.h"
#include "async.h"
#include "ccb_mgr.h"


// path service should hurry up a bit - good controllers don't take much time
// but are very happy to be busy
#define BUS_SERVICE_PRIORITY THREAD_HIGHEST_PRIORITY //10


// 1. to add a bus, prepare a bus_info and finally add a link in xpt_busses
// 2. to remove a bus, first remove link from xpt_busses (grab xpt_devices_mutex
//    to make sure noone in xpt_action currently scans it), remove all devices
//    and the release bus_info
xpt_bus_info *xpt_busses[MAX_PATH_ID + 1];
int xpt_highest_path_id;

static void xpt_do_service( xpt_bus_info *bus )
{
	while( 1 ) {
		SHOW_FLOW0( 3, "" );
		
		if( xpt_check_exec_dpc( bus ))
			continue;
		
		if( xpt_check_exec_service( bus ))
			continue;

		break;
	}
}

static int xpt_service_threadproc( void *arg )
{
	xpt_bus_info *bus = (xpt_bus_info *)arg;
	
	SHOW_FLOW( 3, "bus=%p", bus );
	
	while( 1 ) {
		sem_acquire( bus->start_service, 1 );
		
		SHOW_FLOW0( 3, "1" );
		
		if( bus->shutting_down )
			break;
		
		xpt_do_service( bus );
	}
	
	return 0;
}

static xpt_bus_info *xpt_create_bus( cam_sim_interface *interface, cam_sim_cookie cookie )
{
	xpt_bus_info *bus;
	int res;
	
	SHOW_FLOW0( 3, "" );
	
	bus = kmalloc( sizeof( *bus ));
	if( bus == NULL )
		return NULL;

	bus->lock_count = 0;
	bus->blocked_by_sim = 0;
	bus->shutting_down = false;
	bus->TCQ_support = false;
	bus->do_rescan = false;
	
	bus->path_id = -1;
	bus->sim_cookie = cookie;
	bus->waiting_devices = NULL;
	bus->resubmitted_req = NULL;
	
	bus->dpc_list = NULL;
	
	if( (bus->scan_lun_lock = sem_create( 1, "xpt_scan_lun_lock" )) < 0 ) {
		res = bus->scan_lun_lock;
		goto err6;
	}

	bus->ref_count = 0;	
	/*if( (res = ref_lock_init( &bus->lock )) < 0 )
		goto err5;*/
	if( (bus->global_device = xpt_create_device( bus, XPT_BUS_GLOBAL_ID, 
		XPT_BUS_GLOBAL_ID)) == NULL ) 
	{
		res = ERR_NO_MEMORY;
		goto err5;
	}
	
	bus->start_service = sem_create( 0, "xpt_start_service" );
	if( bus->start_service < 0 ) {
		res = bus->start_service;
		goto err4;
	}
	
	memset( bus->targets, 0, sizeof( bus->targets ));
	
	res = mutex_init( &bus->mutex, "xpt_bus_mutex" );
	if( res != NO_ERROR )
		goto err3;
		
	spinlock_irq_init( &bus->dpc_lock );
	
	res = xpt_init_ccb_alloc( bus );
	if( res != NO_ERROR )
		goto err2;
	
	bus->interface = interface;

	bus->service_thread = thread_create_kernel_thread( "xpt_bus_service", 
		(int (*)())xpt_service_threadproc, bus );
		
	if( bus->service_thread < 0 ) {
		res = bus->service_thread;
		goto err1;
	}
	
	thread_set_priority( bus->service_thread, BUS_SERVICE_PRIORITY );	
	
	thread_resume_thread( bus->service_thread );
	
	return bus;
	
err1:
	xpt_uninit_ccb_alloc( bus );
err2:
	mutex_destroy( &bus->mutex );
err3:	
	sem_delete( bus->start_service );
err4:
	/*ref_lock_uninit( &bus->lock );*/
	xpt_destroy_device( bus->global_device );
err5:
	sem_delete( bus->scan_lun_lock );
err6:
	kfree( bus );
	return NULL;
}

// TBD: it were better if the devfs loads peripheral
// drivers on demand
static void load_periph_drivers()
{
	modules_cookie modules;
	char name[SYS_MAX_PATH_LEN];
	size_t name_len;
	
	SHOW_FLOW0( 3, "" );
	
	// hm, there are other possible directories as well...
	modules = module_open_list( "dev/disk/scsi" );
	while( name_len = sizeof( name ), 
		   read_next_module_name( modules, name, &name_len ) == NO_ERROR )
	{
		void *dummy;
		
		SHOW_FLOW( 3, "loading %s", name );
		
		module_get( name, 0, &dummy );
	}
	
	close_module_list( modules );
}


int xpt_register_SIM( cam_sim_interface *interface, cam_sim_cookie cookie,
					  xpt_bus_info **bus_out )
{
	xpt_bus_info *bus;
	int res;
	int path_id;
	
	SHOW_FLOW0( 3, "" );
	
	// XXX: HACK ALERT
	// manual loading of peripheral drivers must be moved
	// to a better place - at first, I tried to move that to
	// the main init routine, but this leads to recursive
	// modules dependencies 
	// (XPT loads peripheral driver loads driver-XPT loads XPT, ouch)
	load_periph_drivers();
	
	bus = xpt_create_bus( interface, cookie );
	
	if( bus == NULL )
		return ERR_NO_MEMORY;
		
	SHOW_FLOW0( 3, "1" );
		
	recursive_lock_lock( &registration_lock );
	
	SHOW_FLOW0( 3, "2" );
	
	for( path_id = 0; path_id <= MAX_PATH_ID; ++path_id ) {
		if( path_id == XPT_XPT_PATH_ID )
			continue;
			
		if( xpt_busses[path_id] == NULL )
			break;
	}
	
	SHOW_FLOW( 3, "path_id=%i", path_id );
	
	if( path_id > MAX_PATH_ID ) {
		// this isn't really correct, any better idea?
		res = ERR_NO_MEMORY;
		goto err;
	}
		
	// create this link when bus_info is ready to get used!
	xpt_busses[path_id] = bus;
	bus->path_id = path_id;
	*bus_out = bus;
	
	if( path_id > xpt_highest_path_id )
		xpt_highest_path_id = path_id;
	
	recursive_lock_unlock( &registration_lock );
		
	SHOW_FLOW0( 3, "done" );
	
	return path_id;
	
err:
	SHOW_FLOW( 3, "%s", strerror( res ));
	
	return res;
}

static int xpt_destroy_bus( xpt_bus_info *bus )
{
	int target_id, target_lun;
	int retcode;	
	
	bus->interface->unregistered( bus->sim_cookie );
	
	// noone is using this bus now, time to clean it up
	bus->shutting_down = true;
	sem_release( bus->start_service, 1 );
	
	thread_wait_on_thread( bus->service_thread, &retcode );

	sem_delete( bus->start_service );
	mutex_destroy( &bus->mutex );
	sem_delete( bus->scan_lun_lock );
	
	xpt_uninit_ccb_alloc( bus );
	
	return NO_ERROR;
}

static void update_max_path_id()
{
	int id;
	
	for( id = xpt_highest_path_id; id >= 0; --id ) {
		if( xpt_busses[id] != NULL )
			break;
	}
		
	xpt_highest_path_id = id;
}

int xpt_unregister_SIM( int path_id )
{
	int res;
	xpt_bus_info *bus;

	recursive_lock_lock( &registration_lock );
	
	if( path_id >= MAX_PATH_ID || xpt_busses[path_id] == NULL ) {
		res = ERR_INVALID_ARGS;
		goto err;
	}
	
	bus = xpt_busses[path_id];
	xpt_busses[path_id] = NULL;
	
	if( bus->ref_count == 0 )
		res = xpt_destroy_bus( bus );
	else
		res = NO_ERROR;
		
	update_max_path_id();	

err:	
	recursive_lock_unlock( &registration_lock );
	
	return res;
}

xpt_bus_info *xpt_get_bus( uchar path_id )
{
	xpt_bus_info *bus;
	
	SHOW_FLOW( 3, "path_id=%i", path_id );
		
	recursive_lock_lock( &registration_lock );
	
	bus = xpt_busses[path_id];
	if( bus )
		++bus->ref_count;
	
	recursive_lock_unlock( &registration_lock );
	return bus;
}

int xpt_put_bus( xpt_bus_info *bus )
{
	bool to_destroy;
	
	if( bus == NULL )
		return ERR_INVALID_ARGS;
		
	SHOW_FLOW( 3, "path_id=%i", bus->path_id );
		
	recursive_lock_lock( &registration_lock );
	
	--bus->ref_count;
	to_destroy = bus != xpt_busses[bus->path_id];
	
	recursive_lock_unlock( &registration_lock );
	
	if( to_destroy )
		xpt_destroy_bus( bus );
	
	return NO_ERROR;
}
