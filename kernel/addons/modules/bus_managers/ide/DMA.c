/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include "ide_internal.h"
#include "DMA.h"

#include "sync.h"

#define CHECK_DEV_DMA_MODE( infoblock, elem, mode, this_mode, num_modes ) \
if( infoblock->elem ) { \
	mode = this_mode; \
	++num_modes; \
}

static int get_device_dma_mode( ide_device_info *device )
{
	ide_device_infoblock *infoblock = &device->infoblock;
	
	int num_modes, mode;
	
	mode = 0;
	num_modes = 0;
	
	if( infoblock->DMA_supported )
		return -1;
	
	CHECK_DEV_DMA_MODE( infoblock, MDMA0_selected, mode, 0, num_modes );
	CHECK_DEV_DMA_MODE( infoblock, MDMA1_selected, mode, 1, num_modes );
	CHECK_DEV_DMA_MODE( infoblock, MDMA2_selected, mode, 2, num_modes );
	
	if( infoblock->_88_valid ) {
		CHECK_DEV_DMA_MODE( infoblock, UDMA0_selected, mode, 0x10, num_modes );
		CHECK_DEV_DMA_MODE( infoblock, UDMA1_selected, mode, 0x11, num_modes );
		CHECK_DEV_DMA_MODE( infoblock, UDMA2_selected, mode, 0x12, num_modes );
		CHECK_DEV_DMA_MODE( infoblock, UDMA3_selected, mode, 0x13, num_modes );
		CHECK_DEV_DMA_MODE( infoblock, UDMA4_selected, mode, 0x14, num_modes );
		CHECK_DEV_DMA_MODE( infoblock, UDMA5_selected, mode, 0x15, num_modes );
		CHECK_DEV_DMA_MODE( infoblock, UDMA6_selected, mode, 0x16, num_modes );
		
	}	

	if( num_modes != 1 )
		return -1;
	
	return mode;
}

bool configure_DMA( ide_device_info *device )
{
	device->DMA_enabled = device->DMA_supported = 
		device->bus->controller_params.can_DMA &&
		get_device_dma_mode( device ) != -1;
	
	return true;
}

void abort_dma( ide_device_info *device, ide_qrequest *qrequest )
{
	ide_bus_info *bus = device->bus;
	
	bus->controller->finish_dma( bus->channel );
}

// ! doesn't set sense data on error
bool prepare_dma( ide_device_info *device, ide_qrequest *qrequest )
{
	ide_bus_info *bus = device->bus;
	CCB_SCSIIO *request = qrequest->request;
	int res;
	size_t numBytes;
	
	res = bus->controller->prepare_dma( bus->channel, 
		(iovec *)request->cam_sg_list, request->cam_sglist_cnt,
		0, 512, &numBytes, qrequest->is_write );
		
	if( res != NO_ERROR )
		return false;
		
	if( numBytes < qrequest->request->cam_dxfer_len ) {
		abort_dma( device, qrequest );
		return false;
	}
		
	return true;
}

// after launching DMA, we enter async waiting mode
void start_dma( ide_device_info *device, ide_qrequest *qrequest )
{
	ide_bus_info *bus = device->bus;
	
	timer_set_event( qrequest->request->cam_timeout > 0 ? 
		qrequest->request->cam_timeout : IDE_STD_TIMEOUT, 
		TIMER_MODE_ONESHOT, &bus->timer );

	IDE_LOCK( bus );
		
	//bus->state = ide_state_waiting;
	//++bus->wait_id;
	
	// do starting
	
	start_waiting( bus, /*qrequest, */qrequest->request->cam_timeout > 0 ? 
		qrequest->request->cam_timeout : IDE_STD_TIMEOUT, ide_state_async_waiting );
	//IDE_UNLOCK( bus );
}


bool finish_dma( ide_device_info *device )
{
	ide_bus_info *bus = device->bus;
	int dma_res;
	
	dma_res = bus->controller->finish_dma( bus->channel );
	
	return dma_res == NO_ERROR;
}

