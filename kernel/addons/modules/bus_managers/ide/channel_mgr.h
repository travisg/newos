/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __CHANNEL_MGR_H__
#define __CHANNEL_MGR_H__

int connect_channel( const char *name, 
	ide_controller_interface *controller,
	ide_channel_cookie channel,
	ide_controller_params *controller_params,
	ide_bus_info **bus );

int disconnect_channel( ide_bus_info *bus );
void sim_unregistered( ide_bus_info *bus );

#endif
