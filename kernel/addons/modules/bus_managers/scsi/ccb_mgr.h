/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __CCB_MGR_H__
#define __CCB_MGR_H__

CCB_HEADER *xpt_alloc_ccb( xpt_device_info *bus );
void xpt_free_ccb( CCB_HEADER *ccb );

int xpt_init_ccb_alloc( xpt_bus_info *bus );
void xpt_uninit_ccb_alloc( xpt_bus_info *bus );

#endif
