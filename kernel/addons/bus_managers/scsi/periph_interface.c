/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "xpt_internal.h"

#include <string.h>
#include <kernel/module.h>

#include "xpt_io.h"
#include "device_mgr.h"
#include "bus_mgr.h"
#include "bus_interface.h"
#include "ccb_mgr.h"
#include "periph_mgr.h"
#include "blocking.h"

static void xpt_noop( CCB_HEADER *ccb )
{
	xpt_bus_info *bus = ccb->xpt_bus;
	
	ccb->xpt_state = XPT_STATE_SENT;
	bus->interface->action( bus->sim_cookie, ccb );
}

static void xpt_path_inquery( CCB_HEADER *ccb )
{
	CCB_PATHINQ *request = (CCB_PATHINQ *)ccb;
	xpt_bus_info *bus;
	
	SHOW_FLOW( 3, "path_id=%i", ccb->xpt_bus->path_id );
	
	if( ccb->xpt_bus->path_id == XPT_XPT_PATH_ID ) {
		request->cam_hpath_id = xpt_highest_path_id;
		request->cam_version_num = 12;	// XXX 

		// most of the following doesn't really make sense for XPT
		request->cam_hba_inquiry = 0;
		request->cam_target_sprt = 0;
		request->cam_hba_misc = 0;
		request->cam_hba_eng_cnt = 0;
		memset( request->cam_vuhba_flags, 0, sizeof( request->cam_vuhba_flags ));
		request->cam_sim_priv = 0;
		request->cam_async_flags = AC_FOUND_DEVICE | AC_LOST_DEVICE |
			AC_SIM_DEREGISTER | AC_SIM_REGISTER;
		request->cam_initiator_id = 0;
		strncpy( request->cam_sim_vid, "NewOS", SIM_ID );
		strncpy( request->cam_hba_vid, "", HBA_ID );

		ccb->cam_status = CAM_REQ_CMP;
		xpt_done( ccb );
		return;
	}
	
	/*bus = xpt_busses[ccb->cam_path_id];
	
	if( bus == NULL ) {
		ccb->cam_status = CAM_PATH_INVALID;	
		xpt_done( ccb );
		return;
	}*/
	
	bus = ccb->xpt_bus;
	
	ccb->xpt_state = XPT_STATE_SENT;
	bus->interface->action( bus->sim_cookie, ccb );
}

static void xpt_invalid_func( CCB_HEADER *ccb )
{
	ccb->cam_status = CAM_REQ_INVALID;
	xpt_done( ccb );
}

static void xpt_reset_bus( CCB_HEADER *ccb )
{
	ccb->xpt_state = XPT_STATE_SENT;
	ccb->xpt_bus->interface->action( ccb->xpt_bus->sim_cookie, ccb );
}

static void xpt_reset_device( CCB_HEADER *ccb )
{
	ccb->xpt_state = XPT_STATE_SENT;
	ccb->xpt_bus->interface->action( ccb->xpt_bus->sim_cookie, ccb );
}

#define MAKE_BITFIELD8( b0, b1, b2, b3, b4, b5, b6, b7 ) \
	( (b0) | ((b1) << 1) | ((b2) << 2) | ((b3) << 3) | \
	  ((b4) << 4) | ((b5) << 5) | ((b6) << 6) | ((b7) << 7))

#define MAKE_BITFIELD32( b0, b1, b2, b3, b4, b5, b6, b7, \
					   b8, b9, ba, bb, bc, bd, be, bf, \
					   c0, c1, c2, c3, c4, c5, c6, c7, \
					   c8, c9, ca, cb, cc, cd, ce, cf ) \
	(  (uint32)MAKE_BITFIELD8( b0, b1, b2, b3, b4, b5, b6, b7 ) | \
	  ((uint32)MAKE_BITFIELD8( b8, b9, ba, bb, bc, bd, be, bf ) << 8) | \
	  ((uint32)MAKE_BITFIELD8( c0, c1, c2, c3, c4, c5, c6, c7 ) << 16) | \
	  ((uint32)MAKE_BITFIELD8( c8, c9, ca, cb, cc, cd, ce, cf ) << 24))



uint32 is_queued_func[] = {
	MAKE_BITFIELD32
	(0, 1, 0, 0, 0, 0, 0, 0,	// XPT_SCSI_IO
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0),
	MAKE_BITFIELD32
	(0, 1, 0, 0, 0, 0, 0, 0,	// XPT_ENG_EXEC
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 1, 1, 1, 1, 0, 0, 0,	// XPT_TARGET_IO, XPT_ACCEPT_TARG, XPT_CONT_TARG, 
								// XPT_IMMED_NOTIFY
	 0, 0, 0, 0, 0, 0, 0, 0)
};

static inline void xpt_mark_ccb_finished( CCB_HEADER *ccb )
{
	ccb->xpt_state = XPT_STATE_FINISHED;

	if( ccb->cam_func_code < (sizeof( is_queued_func ) / sizeof( is_queued_func[0] )) * 32 &&
		is_queued_func[ccb->cam_func_code >> 5] & ((uint32)1 << (ccb->cam_func_code & 31)) )
	{
		sem_release_etc( ccb->completion_sem, 1, SEM_FLAG_NO_RESCHED );
	}
}


typedef void (*xpt_func_entry)( CCB_HEADER *ccb );

xpt_func_entry xpt_func_list[] =
{
	&xpt_noop,
	&xpt_scsi_io,
	&xpt_get_device_type,
	&xpt_path_inquery,
	&xpt_invalid_func,			/* XPT_REL_SIMQ */
	&xpt_invalid_func,			/* XPT_SASYNC_CB */
	&xpt_invalid_func,			/* XPT_SDEV_TYPE */
	&xpt_scan_bus,
	
	// 8 - 0xf is reserved
	&xpt_invalid_func,
	&xpt_invalid_func,
	&xpt_invalid_func,
	&xpt_invalid_func,
	&xpt_invalid_func,
	&xpt_invalid_func,
	&xpt_invalid_func,
	&xpt_invalid_func,

	&xpt_abort,	
	&xpt_reset_bus,
	&xpt_reset_device,
	&xpt_term_io,
	&xpt_scan_lun
};

enum { 
	func_needs_bus = 1,
	func_needs_lun,
	func_needs_lun_any
};

unsigned char xpt_func_needs[sizeof( xpt_func_list ) / sizeof( xpt_func_entry )] =
{
	func_needs_bus,
	func_needs_lun,
	func_needs_lun,
	func_needs_bus,		// special case of path_id=255 is handled in sim_action
	0,
	0,
	0,
	func_needs_bus,
	
	0, 0, 0, 0, 0, 0, 0, 0,
	
	0,					// not clear: does abort needs its own path/target/lun?
	func_needs_bus,
	func_needs_lun,
	func_needs_lun,
	func_needs_lun_any
};

void xpt_done_nonio( CCB_HEADER *ccb )
{
	xpt_mark_ccb_finished( ccb );
}


void xpt_action( CCB_HEADER *ccb )
{
	SHOW_FLOW( 3, "cam_func_code=%x, bus=%p, path=%i, target=%i, lun=%i", 
		ccb->cam_func_code, ccb->xpt_bus, 
		ccb->cam_path_id, ccb->cam_target_id, ccb->cam_target_lun );
	
	if( ccb->xpt_state != XPT_STATE_FINISHED ) 
		goto ccb_not_ready;
	
	ccb->xpt_state = XPT_STATE_SENT;
	
	if( ccb->xpt_bus->disconnected )
		goto invalid_bus;
		
	SHOW_FLOW( 3, "path_id=%i", ccb->xpt_bus->path_id );
	
	if( ccb->cam_func_code < sizeof( xpt_func_list ) / sizeof( xpt_func_entry )) {
		unsigned int needs;
		
		SHOW_FLOW0( 3, "standard call" );
		
		needs = xpt_func_needs[ccb->cam_func_code];
		
		SHOW_FLOW( 3, "needs=%x", needs );
		
		if( needs == func_needs_lun ) {
			if( ccb->xpt_device == ccb->xpt_bus->global_device )
				goto invalid_ccb;
		
			/*
			// this is not perfect as not atomic - we do "our best" to deny
			// accessing removed or replaced devices
			if( ccb->xpt_device->temporary )
				goto invalid_device;*/
				
		} else if( needs == func_needs_lun_any ) {
			if( ccb->xpt_device == ccb->xpt_bus->global_device )
				goto invalid_ccb;
			
		} else if( needs == func_needs_bus ) {
		} 
		
		SHOW_FLOW0( 3, "go" );

		xpt_func_list[ccb->cam_func_code]( ccb );
		
	} else if( ccb->cam_func_code < XPT_VUNIQUE ) {
		ccb->cam_status = CAM_REQ_INVALID;
		xpt_mark_ccb_finished( ccb );
		return;
		
	} else {
		ccb->xpt_bus->interface->action( ccb->xpt_bus->sim_cookie, ccb );
	}
	
	return;

	
invalid_bus:
	ccb->cam_status = CAM_PATH_INVALID;
	goto err;
	
invalid_ccb:
	ccb->cam_status = CAM_REQ_INVALID;
	goto err;
	
//invalid_device:
	ccb->cam_status = CAM_DEV_NOT_THERE;
	
err:
	xpt_mark_ccb_finished( ccb );
	return;

ccb_not_ready:	
	panic( "Passed ccb to xpt_action that isn't ready (state = %i)\n", ccb->xpt_state );
	return;
}


cam_for_driver_interface driver_interface =
{
	xpt_get_bus,
	xpt_put_bus,
	
	xpt_get_device,
	xpt_put_device,
	
	xpt_alloc_ccb,
	xpt_free_ccb,
	xpt_action,
	
	xpt_register_driver,
	xpt_unregister_driver,
};


module_header xpt_driver_module = {
	CAM_FOR_DRIVER_MODULE_NAME,
	MODULE_CURR_VERSION,
	0,
	&driver_interface,
	
	xpt_driver_sim_init,
	xpt_driver_sim_uninit
};
