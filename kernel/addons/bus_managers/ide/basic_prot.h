/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __BASIC_PROT_H__
#define __BASIC_PROT_H__


bool ide_wait( ide_device_info *device, int mask, int not_mask, 
	bool check_err, bigtime_t timeout );
bool wait_for_drq( ide_device_info *device );
bool wait_for_drqdown( ide_device_info *device );
bool wait_for_drdy( ide_device_info *device );

bool send_command( ide_device_info *device, 
	bool need_drdy, bigtime_t timeout, ide_bus_state new_state );
bool device_start_service( ide_device_info *device, int *tag );
bool reset_bus( ide_device_info *device );
	
bool check_service_req( ide_device_info *device );

static inline ide_device_info *get_current_device( ide_bus_info *bus )
{
	ide_task_file tf;

	bus->controller->read_command_block_regs( bus->channel, &tf, 
		ide_mask_device_head );
	
	return bus->devices[tf.lba.device];
}

static inline int device_released_bus( ide_device_info *device )
{
	ide_bus_info *bus = device->bus;

	bus->controller->read_command_block_regs( 
		bus->channel, &device->tf, ide_mask_sector_count );
		
	return device->tf.queued.release;
}


void spin( bigtime_t duration );

#endif
