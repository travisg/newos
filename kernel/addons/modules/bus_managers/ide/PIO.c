/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#include "ide_internal.h"
#include "PIO.h"

#include <string.h>

#include "ide_sim.h"

void prep_PIO_transfer( ide_device_info *device, ide_qrequest *qrequest )
{
	device->left_sg_elem = qrequest->request->cam_sglist_cnt;
	device->cur_sg_elem = qrequest->request->cam_sg_list;
	device->cur_sg_ofs = 0;
	device->has_odd_byte = false;
	qrequest->request->cam_resid = qrequest->request->cam_dxfer_len;
}

static inline int transfer_PIO_virtcont( ide_device_info *device, char *virt_addr, int length,
	bool write, int *transferred )
{
	ide_bus_info *bus = device->bus;
	ide_controller_interface *controller = bus->controller;
	ide_channel_cookie cookie = bus->channel;

	if( write ) {
		if( device->has_odd_byte ) {
			uint8 buffer[2];

			buffer[0] = device->odd_byte;
			buffer[1] = *virt_addr++;

			controller->write_pio_16( cookie, (uint16 *)buffer, 1, false );

			--length;
		}

		controller->write_pio_16( cookie, (uint16 *)virt_addr, length / 2, false );

		virt_addr += length & ~1;

		device->has_odd_byte = (length & 1) != 0;

		if( device->has_odd_byte )
			device->odd_byte = *virt_addr;

	} else {
		if( device->has_odd_byte ) {
			*virt_addr++ = device->odd_byte;
			--length;
		}

		controller->read_pio_16( cookie, (uint16 *)virt_addr, length / 2, false );

		virt_addr += length & ~1;

		device->has_odd_byte = (length & 1) != 0;

		if( device->has_odd_byte ) {
			uint8 buffer[2];

			controller->read_pio_16( cookie, (uint16 *)buffer, 1, false );
			*virt_addr = buffer[0];
			device->odd_byte = buffer[1];
		}
	}

	*transferred += length;
	return NO_ERROR;
}

static inline int transfer_PIO_physcont( ide_device_info *device, addr_t phys_addr, int length,
	bool write, int *transferred )
{
	while( length > 0 ) {
		addr_t virt_addr;
		int page_left, cur_len;
		int err;

		if( vm_get_physical_page( phys_addr, &virt_addr, PHYSICAL_PAGE_CAN_WAIT ) != NO_ERROR )
		{
			// ouch: this should never ever happen
			set_sense( device, SCSIS_KEY_HARDWARE_ERROR, SCSIS_ASC_INTERNAL_FAILURE );
			return ERR_GENERAL;
		}

		page_left = PAGE_SIZE - phys_addr % PAGE_SIZE;

		cur_len = min( page_left, length );

		err = transfer_PIO_virtcont( device, (char *)virt_addr,
			cur_len, write, transferred );

		if( err != NO_ERROR ) {
			vm_put_physical_page( virt_addr );
			return err;
		}

		length -= cur_len;
		phys_addr += length;

		vm_put_physical_page( virt_addr );
	}

	return NO_ERROR;
}

static inline int transfer_PIO_block( ide_device_info *device, int length, bool write,
	int *transferred )
{
	while( length > 0 ) {
		int left_bytes, cur_len;
		int err;

		if( device->left_sg_elem == 0 ) {
			return ERR_TOO_BIG;
		}

		left_bytes = device->cur_sg_elem->cam_sg_count - device->cur_sg_ofs;

		cur_len = min( left_bytes, length );

		err = transfer_PIO_physcont( device,
			(addr_t)device->cur_sg_elem->cam_sg_address + device->cur_sg_ofs,
			cur_len, write, transferred );

		if( err != NO_ERROR )
			return err;

		if( left_bytes <= length ) {
			device->cur_sg_ofs = 0;
			++device->cur_sg_elem;
			--device->left_sg_elem;
		} else {
			device->cur_sg_ofs += cur_len;
		}

		length -= cur_len;
	}

	return NO_ERROR;
}

static bool write_discard_PIO( ide_device_info *device, int length )
{
	ide_bus_info *bus = device->bus;
	uint8 buffer[32];

	memset( buffer, 0, sizeof( buffer ));

	while( length > 0 ) {
		int cur_len;

		if( device->has_odd_byte ) {
			buffer[0] = device->odd_byte;
			device->has_odd_byte = false;
		}

		cur_len = min( length + 1, (int)(sizeof( buffer ))) / 2;

		bus->controller->write_pio_16( bus->channel, (uint16 *)buffer, cur_len, false );

		length -= cur_len * 2;

		buffer[0] = 0;
	}

	return true;
}

static bool read_discard_PIO( ide_device_info *device, int length )
{
	ide_bus_info *bus = device->bus;
	uint8 buffer[32];

	device->has_odd_byte = false;

	while( length > 0 ) {
		int cur_len;

		cur_len = min( length, (int)(sizeof( buffer ) / sizeof( uint16 )));

		bus->controller->read_pio_16( bus->channel, (uint16 *)buffer, cur_len, false );

		length -= cur_len;
	}

	return true;
}

// there are 3 possible results
// NO_ERROR - everything's nice and groovy
// ERR_TOO_BIG - data buffer was too short, remaining data got discarded
// ERR_GENERAL - something serious went wrong, sense data was set
int write_PIO_block( ide_qrequest *qrequest, int length )
{
	int transferred;
	int err;

	transferred = 0;
	err = transfer_PIO_block( qrequest->device, length, true, &transferred );

	if( err == ERR_TOO_BIG ) {
		if( !write_discard_PIO( qrequest->device, max( length - transferred, 0 )))
			return ERR_GENERAL;
	}

	qrequest->request->cam_resid -= length;

	return err;
}

// see write_PIO_block
int read_PIO_block( ide_qrequest *qrequest, int length )
{
	int transferred;
	int err;

	transferred = 0;
	err = transfer_PIO_block( qrequest->device, length, false, &transferred );

	if( err == ERR_TOO_BIG ) {
		if( !read_discard_PIO( qrequest->device, max( length - transferred, 0 )))
			return ERR_GENERAL;
	}

	qrequest->request->cam_resid -= length;

	return err;
}
