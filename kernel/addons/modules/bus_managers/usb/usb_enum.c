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

#include <kernel/bus/usb/usb.h>
#include <kernel/bus/usb/usb_hc.h>
#include "usb_priv.h"

#define debug_level_flow 10
#define debug_level_error 10
#define debug_level_info 10

#define DEBUG_MSG_PREFIX "USB_ENUM - "

#include <kernel/debug_ext.h>

static usb_endpoint_descriptor null_endpoint = {
	sizeof(usb_endpoint_descriptor),
	USB_DESCRIPTOR_ENDPOINT,
	0, 0, 8, 0
};

int usb_enumerator_thread(void *hc_struct)
{
	usb_hc *hc = (usb_hc *)hc_struct;
	int err;

	SHOW_FLOW(1, "starting up on hc %p, cookie %p", hc, hc->hc_cookie);

	// create the null endpoint for address 0 so we can talk to unconfigured devices
	create_usb_pipe(hc, NULL, &hc->default_lowspeed_pipe, &null_endpoint, 0, true);
	create_usb_pipe(hc, NULL, &hc->default_fullspeed_pipe, &null_endpoint, 0, false);

	// create a root hub device
	err = create_usb_device(hc, &hc->roothub, NULL, 0, false);
	if(err < 0) {
		SHOW_ERROR0(1, "error creating root hub device, bailing...");
		return -1;
	}

	// set up the hub
	err = setup_usb_hub(hc->roothub);
	if(err < 0) {
		SHOW_ERROR0(1, "error setting up root hub, bailing...");
		return -1;
	}

	for(;;) {
		thread_snooze(1000000);
		// XXX finish
	}

	return 0;
}

