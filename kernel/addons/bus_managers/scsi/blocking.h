/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __BLOCKING_H__
#define __BLOCKING_H__


static inline void xpt_set_device_overflow( xpt_device_info *device )
{
	device->lock_count += device->sim_overflow ^ 1;
	device->sim_overflow = true;
}

static inline void xpt_set_bus_overflow( xpt_bus_info *bus )
{
	bus->lock_count += bus->sim_overflow ^ 1;
	bus->sim_overflow = true;
}

static inline bool xpt_can_service_bus( xpt_bus_info *bus )
{
	return bus->lock_count > bus->sim_overflow + bus->blocked_by_sim;
}

static inline void xpt_clear_device_overflow( xpt_device_info *device )
{
	device->lock_count -= device->sim_overflow;
	device->sim_overflow = false;
}

static inline void xpt_clear_bus_overflow( xpt_bus_info *bus )
{
	bus->lock_count -= bus->sim_overflow;
	bus->sim_overflow = false;
}

int xpt_block_bus( xpt_bus_info *bus );
int xpt_unblock_bus( xpt_bus_info *bus );
int xpt_block_device( xpt_bus_info *bus, int target_id, int lun );
int xpt_unblock_device( xpt_bus_info *bus, int target_id, int lun );

int xpt_cont_send_bus( xpt_bus_info *bus );
int xpt_cont_send_device( xpt_bus_info *bus, int target_id, int lun );

#endif