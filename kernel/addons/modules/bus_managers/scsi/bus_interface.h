/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __BUS_INTERFACE_H__
#define __BUS_INTERFACE_H__

#include <kernel/debug.h>

#include "xpt_io.h"
#include "periph_interface.h"

static inline void xpt_done( CCB_HEADER *ccb )
{
    SHOW_FLOW( 3, "cmd=%x, status=%x", (int)ccb->cam_func_code, (int)ccb->cam_status );

    if ( ccb->xpt_state != XPT_STATE_SENT ) {
        panic( "Unsent ccb was reported as done\n" );
        return;
    }

    if ( ccb->cam_status == CAM_REQ_INPROG ) {
        panic( "ccb with status \"Request in Progress\" was reported as done\n" );
        return;
    }

    if ( ccb->cam_func_code == XPT_SCSI_IO ) {
        xpt_done_io( ccb );

    } else {
        xpt_done_nonio( ccb );
    }
}


#endif
