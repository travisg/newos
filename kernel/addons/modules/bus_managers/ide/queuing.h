/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __QUEUING_H__
#define __QUEUING_H__

void reset_timeouts( ide_device_info *device );
bool send_abort_queue( ide_device_info *device );
bool try_service( ide_device_info *device );

void reconnect_timeout_worker( ide_bus_info *bus, void *arg );
int reconnect_timeout( void * arg );

bool initialize_qreq_array( ide_device_info *device, int queue_depth );
void destroy_qreq_array( ide_device_info *device );

#endif
