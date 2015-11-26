/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __DEVICE_MGR_H
#define __DEVICE_MGR_H


bool initialize_buffer( ide_device_info *device, size_t buffer_size );
void scan_device_worker( ide_bus_info *bus, void *arg );

#endif
