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

#define DEBUG_MSG_PREFIX "USB - "

#include <kernel/debug_ext.h>

usb_bus *usb;

static int
hc_init_callback(void *hooks, void *hc_cookie)
{
	usb_hc *hc;

	SHOW_FLOW(1, "cookie %p", hc_cookie);

	hc = (usb_hc *)kmalloc(sizeof(usb_hc));
	if(!hc)
		return ERR_NO_MEMORY;

	memset(hc, 0, sizeof(*hc));

	hc->hooks = hooks;
	hc->hc_cookie = hc_cookie;

	// add it to the list of host controllers
	hc->next = usb->hc_list;
	usb->hc_list = hc;

	// create an enumerator thread
	hc->enumerator_thread = thread_create_kernel_thread("usb bus enumerator", &usb_enumerator_thread, hc);
	thread_set_priority(hc->enumerator_thread, THREAD_MIN_RT_PRIORITY);
	thread_resume_thread(hc->enumerator_thread);

	return NO_ERROR;
}

static int
load_hc_modules()
{
	char module_name[SYS_MAX_PATH_LEN];
	size_t bufsize;
	modules_cookie cookie;
	struct usb_hc_module_hooks *hooks;
	int err;
	int count = 0;

	// scan through the host controller module dir and load all of them
	cookie = module_open_list(USB_HC_MODULE_NAME_PREFIX);
	bufsize = sizeof(module_name);
	while(read_next_module_name(cookie, module_name, &bufsize) >= NO_ERROR) {
		bufsize = sizeof(module_name); // reset this for the next iteration

		err = module_get(module_name, 0, (void **)&hooks);
		if(err < 0)
			continue;

		err = hooks->init_hc(&hc_init_callback, hooks);
		if(err < 0) {
			module_put(module_name); // it failed, put it away
		}

		count++;
	}
	close_module_list(cookie);

	return count;
}

static int
usb_module_init(void)
{
	usb = kmalloc(sizeof(usb_bus));
	if(!usb)
		return ERR_NO_MEMORY;

	load_hc_modules();

	return NO_ERROR;
}

static int
usb_module_uninit(void)
{
	return NO_ERROR;
}

static struct usb_module_hooks usb_hooks = {
	NULL,
};

static module_header usb_module_header = {
	USB_BUS_MODULE_NAME,
	MODULE_CURR_VERSION,
	MODULE_KEEP_LOADED,
	&usb_hooks,
	&usb_module_init,
	&usb_module_uninit
};

module_header *modules[]  = {
	&usb_module_header,
	NULL
};

