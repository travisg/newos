/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __ASYNC_H__
#define __ASYNC_H__

int xpt_call_async( xpt_bus_info *bus, int target_id, int lun, 
	int code, uchar *data, int data_len );
int xpt_call_async_all( xpt_bus_info *bus, int target_id, int lun, 
	int code, uchar *data, int data_len, bool from_xpt );
int xpt_flush_async( xpt_bus_info *bus );

#endif
