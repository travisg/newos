/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include "xpt_internal.h"
#include "async.h"

#include <kernel/debug.h>

#include "device_mgr.h"
#include "periph_mgr.h"


static void xpt_call_periph_async( xpt_bus_info *bus, int target_id, int lun, 
	int code, void *data, int data_len )
{	
	xpt_periph_info *periph;
	
	mutex_lock( &xpt_periphs_mutex );
	
	for( periph = xpt_periphs; periph; periph = periph->next ) {
		periph->interface->async( periph->periph_cookie, 
			bus->path_id, target_id, lun, code, data, 
			data_len );
	}
	
	mutex_unlock( &xpt_periphs_mutex );
}


int xpt_call_async_all( xpt_bus_info *bus, int target_id, int lun, 
	int code, uchar *data, int data_len, bool from_xpt )
{
	SHOW_FLOW( 3, "%i:%i:%i; code=%i", (int)bus->path_id, (int)target_id, (int)lun, (int)code );
	
	switch( code ) {
	case AC_BUS_RESET:
		xpt_call_periph_async( bus, -1, -1, code, NULL, 0 );
		break;
	case AC_UNSOL_RESEL:
		// device attempted to continue a command that doesn't exist
		xpt_call_periph_async( bus, target_id, lun, code, NULL, 0 );
		break;
	case AC_SCSI_AEN:
		// received Asynchronous Event Notification
		xpt_call_periph_async( bus, target_id, lun, code, data, data_len );
		break;
	case AC_SENT_BDR:
		// sent bus device reset
		xpt_call_periph_async( bus, target_id, 0, code, NULL, 0 );
		break;
	case AC_SIM_REGISTER:
		xpt_call_periph_async( bus, -1, -1, code, data, data_len );
		break;
	case AC_SIM_DEREGISTER:
		xpt_call_periph_async( bus, -1, -1, code, data, data_len );
		break;
	case AC_FOUND_DEVICE:
	case AC_LOST_DEVICE:
		if( from_xpt )
			xpt_call_periph_async( bus, target_id, lun, code, NULL, 0 );
		else {
			// we check this condition ourself and inform peripheral drivers
			// if something has really changed
			xpt_schedule_busscan( bus );
		}
		break;
	default:
		dprintf( "received invalid asynchronous event %i\n", code );
		return ERR_INVALID_ARGS;
	}
	
	return NO_ERROR;
}

int xpt_call_async( xpt_bus_info *bus, int target_id, int lun, 
	int code, uchar *data, int data_len )
{
	return xpt_call_async_all( bus, target_id, lun, code, data, data_len, false );
}

int xpt_flush_async( xpt_bus_info *bus )
{
	// this is so rarely used that I don't want to add extra overhead
	// to the service thread
	while( bus->dpc_list )
		thread_snooze( 10000 );
		
	return NO_ERROR;
}
