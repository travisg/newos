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
	hc_endpoint *null_hc_endpoint_lowspeed;
	hc_endpoint *null_hc_endpoint;

	SHOW_FLOW(1, "starting up on hc %p, cookie %p", hc, hc->hc_cookie);

	// create the null endpoint for address 0 so we can talk to unconfigured devices
	hc->hooks->create_endpoint(hc->hc_cookie, &null_hc_endpoint_lowspeed, &null_endpoint, 0, 1);
	hc->hooks->create_endpoint(hc->hc_cookie, &null_hc_endpoint, &null_endpoint, 0, 0);

	for(;;) {
		thread_snooze(1000000);
		// XXX finish
	}

	return 0;
}

