/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "scsi_periph_int.h"

#include <kernel/heap.h>
#include <kernel/debug.h>

cam_periph_interface periph_interface = {
	(void (*)( cam_periph_cookie, int, int, int, 
		int, const void *, int ))					scsi_async
};


int scsi_register_driver( scsi_periph_callbacks *callbacks, 
	periph_cookie periph_cookie, scsi_driver_info **driver_out )
{
	scsi_driver_info *driver;
	
	SHOW_FLOW0( 3, "" );
	
	driver = kmalloc( sizeof( *driver ));
	if( driver == NULL )
		return ERR_NO_MEMORY;
		
	driver->callbacks = callbacks;
	driver->periph_cookie = periph_cookie;
	driver->devices = NULL;
	
	driver->xpt_cookie = xpt->register_driver( &periph_interface, 
		(cam_periph_cookie)driver );
	if( driver->xpt_cookie == NULL ) {
		kfree( driver );
		return ERR_NO_MEMORY;
	}
	
	SHOW_FLOW0( 3, "1" );
	
	*driver_out = driver;
	
	return NO_ERROR;
}

int scsi_unregister_driver( scsi_driver_info *driver )
{
	int res;
	
	if( driver->devices )
		panic( "Tried to unregister driver with active devices\n" );
		
	if( (res = xpt->unregister_driver( driver->xpt_cookie )) != NO_ERROR )
		return res;
		
	kfree( driver );
	
	return NO_ERROR;
}
