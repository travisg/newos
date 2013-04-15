/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/lock.h>
#include <kernel/module.h>
#include <kernel/heap.h>
#include <kernel/thread.h>
#include <string.h>

#include <kernel/bus/usb/usb.h>
#include <kernel/bus/usb/usb_hc.h>
#include "usb_priv.h"

#define debug_level_flow 10
#define debug_level_error 10
#define debug_level_info 10

#define DEBUG_MSG_PREFIX "USB_HUB - "

#include <kernel/debug_ext.h>

static void usb_hub_interrupt_callback(usb_hc_transfer *td, void *cookie);
static int schedule_hub_interrupt(usb_device *device);

static void usb_hub_interrupt_callback(usb_hc_transfer *td, void *cookie)
{
	usb_device *device = (usb_device *)cookie;

	switch(td->status) {
		case 0:
			// success!
			// XXX here

			// set up the next transfer
			schedule_hub_interrupt(device);
			break;
		default:
			;
	}

	kfree(td);

	SHOW_FLOW(5, "callback on device %p", device);
}

static int schedule_hub_interrupt(usb_device *device)
{
	return queue_interrupt_transfer(device->active_config->interfaces->active_interface->endpoints->pipe,
		device->hub_data, device->active_config->interfaces->active_interface->endpoints->desc->max_packet_size,
		&usb_hub_interrupt_callback, device);
}

int setup_usb_hub(usb_device *device)
{
	int err;
	int i;

	// check to see if this is a hub
	if(device->desc.device_class != 0x09) {
		SHOW_ERROR0(1, "device is not a hub");
		return ERR_INVALID_ARGS;
	}

	// set the first configuration
	err = set_usb_configuration(device, device->configs);
	if(err < 0) {
		return err;
	}

	device->hub_desc = kmalloc(sizeof(usb_hub_descriptor));
	if(!device->hub_desc)
		return ERR_NO_MEMORY;

	// grab the hub descriptor, short version
	err = send_usb_request(device, USB_REQTYPE_CLASS|USB_REQTYPE_DEVICE_IN, USB_REQUEST_GET_DESCRIPTOR,
		0, 0, 7 /* dont know how many ports the hub has yet */, device->hub_desc);
	if(err < 0) {
		kfree(device->hub_desc);
		return err;
	}

	// grab the hub descriptor, long version
	err = send_usb_request(device, USB_REQTYPE_CLASS|USB_REQTYPE_DEVICE_IN, USB_REQUEST_GET_DESCRIPTOR,
		0, 0, device->hub_desc->length, device->hub_desc);
	if(err < 0) {
		kfree(device->hub_desc);
		return err;
	}

	// set up the structure
	device->is_hub = true;
	device->num_children = 0;
	SHOW_FLOW(5, "found hub: address %d, num_ports %d", device->address, device->hub_desc->num_ports);

	// power each of the ports
	for(i = 1; i <= device->hub_desc->num_ports; i++) {
		send_usb_request(device, USB_REQTYPE_CLASS|USB_REQTYPE_OTHER_OUT, USB_REQUEST_SET_FEATURE,
			USB_HUB_PORT_POWER, i, 0, NULL);
	}

	// wait long enough for the hub to fully power up
	thread_snooze(1000 * (10 + device->hub_desc->power_delay * 2));

	for(i = 1; i <= device->hub_desc->num_ports; i++) {
		char argh[4];
		send_usb_request(device, USB_REQTYPE_CLASS|USB_REQTYPE_OTHER_IN, USB_REQUEST_GET_STATUS,
			0, i, sizeof(argh), argh);
	}

	// set up interrupt request
	schedule_hub_interrupt(device);

	return NO_ERROR;
}

