/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __ATAPI_H__
#define __ATAPI_H__


bool prep_atapi( ide_device_info *device );

void send_packet( ide_device_info *device, ide_qrequest *qrequest, bool write );
void packet_dpc( ide_qrequest *qrequest );
void adjust_atapi_result( ide_device_info *device, ide_qrequest *qrequest );
void atapi_exec_io( ide_device_info *device, ide_qrequest *qrequest );


#endif
