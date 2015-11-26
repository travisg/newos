/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __DMA_H__
#define __DMA_H__


#define MAX_DMA_FAILURES 3

bool prepare_dma( ide_device_info *device, ide_qrequest *qrequest );
void start_dma( ide_device_info *device, ide_qrequest *qrequest );
bool finish_dma( ide_device_info *device );
void abort_dma( ide_device_info *device, ide_qrequest *qrequest );

bool configure_DMA( ide_device_info *device );

#endif
