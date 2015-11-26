/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __BUS_MGR_H__
#define __BUS_MGR_H__

extern int xpt_highest_path_id;
extern xpt_bus_info *xpt_busses[MAX_PATH_ID + 1];

int xpt_register_SIM( cam_sim_interface *interface, cam_sim_cookie cookie,
                      xpt_bus_info **bus_out );
int xpt_unregister_SIM( int path_id );

xpt_bus_info *xpt_get_bus( uchar path_id );
int xpt_put_bus( xpt_bus_info *bus );

#endif
