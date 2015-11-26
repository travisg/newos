#ifndef __USB_PRIV_H
#define __USB_PRIV_H

#include <kernel/bus/usb/usb_spec.h>

/* heirarchy of usb structures:
 *
 * usb_bus:    one in the entire system. contains a list of host controllers.
 * usb_hc:     one per detected host controller, contains root pointer to tree
 *             made up of usb_device structs.
 * usb_device: one per device, holds a list of usb_configuration structs,
 *             and the currently active one. If a hub, contains the list of all of it's
 *             children.
 * usb_configuration: one or more per device. holds a list of usb_interface structs,
 *             and the currently active one.
 * usb_interface: one or more per device. holds a list of usb_endpoint structs.
 * usb_endpoint: wrapper around a usb_pipe.
 *
 */

typedef struct usb_bus {
    struct usb_hc *hc_list;
} usb_bus;

extern usb_bus *usb;

// one per host controller
typedef struct usb_hc {
    struct usb_hc *next;

    // callback stuff
    struct usb_hc_module_hooks *hooks;
    void *hc_cookie;

    // each hc instance has an enumerator thread
    thread_id enumerator_thread;

    // a pipe used to talk to devices as they're initially brought up
    struct usb_pipe *default_lowspeed_pipe;
    struct usb_pipe *default_fullspeed_pipe;

    // device list
    struct usb_device *roothub;

    // bitmask of addresses in use
    uint8 device_address_bitmask[128/8];
} usb_hc;

// one per device
typedef struct usb_device {
    usb_device_descriptor desc;
    int address;    // 0..127
    usb_hc *hc;

    // control pipe
    struct usb_pipe *control_pipe;

    // configuration list
    struct usb_configuration *configs;
    struct usb_configuration *active_config;

    // hub stuff
    bool is_hub;
    usb_hub_descriptor *hub_desc;
    int num_children;
    struct usb_device *parent;
    struct usb_device *child_list;
    struct usb_device *next_sibling;
    uint8 hub_data[8];

} usb_device;

// one per configuration
typedef struct usb_configuration {
    // NOTE: freeing this will free all of the desc pointers for sub interfaces and endpoints
    // memory is allocated all at once.
    usb_configuration_descriptor *desc;

    // next in list of configurations per device
    struct usb_configuration *next;

    struct usb_device *dev;

    // list of interfaces
    struct usb_interface *interfaces;
} usb_configuration;

// one per interface
typedef struct usb_interface {
    // next in list of interfaces per configuration
    struct usb_interface *next;

    usb_interface_descriptor *desc;

    // list of alternate interfaces
    struct usb_interface *alternates;
    struct usb_interface *active_interface;

    // list of endpoints
    struct usb_endpoint *endpoints;
} usb_interface;

// one per endpoint
typedef struct usb_endpoint {
    // next in list of endpoints
    struct usb_endpoint *next;

    usb_endpoint_descriptor *desc;

    struct usb_pipe *pipe;
} usb_endpoint;

typedef struct usb_pipe {
    hc_endpoint *endpoint;
    bool lowspeed;
    struct usb_hc *hc;
} usb_pipe;

// local functions
int usb_enumerator_thread(void *hc_struct);

// usb_pipe.c
int create_usb_pipe(usb_hc *hc, usb_interface *iface, usb_pipe **p,
                    usb_endpoint_descriptor *desc, int address, bool lowspeed);
int send_usb_request(usb_device *device, uint8 request_type, uint8 request, uint16 value,
                     uint16 index, uint16 len, void *data);
int queue_interrupt_transfer(usb_pipe *pipe, void *data, size_t len, void (*callback)(usb_hc_transfer *, void *), void *cookie);

// usb_device.c
int create_usb_device(usb_hc *hc, usb_device **_device, usb_device *parent, int port, bool lowspeed);
int get_usb_descriptor(usb_device *device, uint8 type, uint8 index, uint16 lang, void *data, size_t len);
int create_usb_interface(usb_device *device, usb_interface *interface);
int set_usb_configuration(usb_device *device, usb_configuration *config);

// usb_hub.c
int setup_usb_hub(usb_device *device);

#endif

