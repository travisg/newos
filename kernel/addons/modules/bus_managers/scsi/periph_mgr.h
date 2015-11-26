/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __PERIPH_MGR_H__
#define __PERIPH_MGR_H__

extern mutex xpt_periphs_mutex;
extern xpt_periph_info *xpt_periphs;

xpt_periph_info *xpt_register_driver( cam_periph_interface *interface,
                                      cam_periph_cookie periph_cookie );
int xpt_unregister_driver( xpt_periph_info *periph );

#endif
