#ifndef __USB_PRIV_H
#define __USB_PRIV_H

typedef struct usb_bus {
	struct usb_hc *hc_list;
} usb_bus;

extern usb_bus *usb;

typedef struct usb_hc {
	struct usb_hc *next;

	// callback stuff
	struct usb_hc_module_hooks *hooks;
	void *hc_cookie;

	// each hc instance has an enumerator thread
	thread_id enumerator_thread;
} usb_hc;

// local functions
int usb_enumerator_thread(void *hc_struct);

#endif

