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

#define DEBUG_MSG_PREFIX "USB_DEVICE - "

#include <kernel/debug_ext.h>

static int set_new_usb_address(usb_device *device);
static int load_device_configs(usb_device *device);

int create_usb_device(usb_hc *hc, usb_device **_device, usb_device *parent, int port, bool lowspeed)
{
    usb_device *device;
    int err;

    SHOW_FLOW(5, "hc %p, parent %p, port %d, lowspeed %d", hc, parent, port, lowspeed);

    device = kmalloc(sizeof(usb_device));
    if (!device)
        return ERR_NO_MEMORY;
    memset(device, 0, sizeof(usb_device));

    device->hc = hc;
    device->address = 0; // defaults to zero
    device->parent = parent;

    if (lowspeed)
        device->control_pipe = hc->default_lowspeed_pipe;
    else
        device->control_pipe = hc->default_fullspeed_pipe;

    // get the descriptor from the device
    err = get_usb_descriptor(device, USB_DESCRIPTOR_DEVICE, 0, 0, &(device->desc), 8);
    if (err < 0)
        goto err;

    // find a new address and set it
    err = set_new_usb_address(device);
    if (err < 0)
        goto err;

    thread_snooze(5000); // let the device settle

    // now that we know the size of the descriptor, grab the full size
    err = get_usb_descriptor(device, USB_DESCRIPTOR_DEVICE, 0, 0, &(device->desc), device->desc.length);
    if (err < 0)
        goto err;

    // load the device configurations
    err = load_device_configs(device);

    *_device = device;
    return NO_ERROR;

err:
    kfree(device);
    return err;
}

int get_usb_descriptor(usb_device *device, uint8 type, uint8 index, uint16 lang, void *data, size_t len)
{
    return send_usb_request(device, USB_REQTYPE_DEVICE_IN, USB_REQUEST_GET_DESCRIPTOR,
                            (type << 8) | index, lang, len, data);
}

static int allocate_usb_address(usb_hc *hc)
{
    int i;

    // find a new address and mark it in use
    // XXX lock this
    for (i = 1; i < 128; i++) {
        if ((hc->device_address_bitmask[i/8] & (1 << (i%8))) == 0) {
            hc->device_address_bitmask[i/8] |= 1 << (i%8);
            break;
        }
    }
    if (i >= 128)
        return ERR_NO_MORE_HANDLES;

    return i;
}

static void free_usb_address(usb_hc *hc, int address)
{
    hc->device_address_bitmask[address/8] &= ~(1 << (address%8));
}

static int set_new_usb_address(usb_device *device)
{
    usb_hc *hc = device->hc;
    int new_address;
    usb_endpoint_descriptor endpoint_desc;
    usb_pipe *pipe;
    int err;

    // find a new address for this device
    new_address = allocate_usb_address(hc);
    if (new_address < 0)
        return new_address;

    // set up a new endpoint descriptor
    endpoint_desc.length = 8;
    endpoint_desc.descriptor_type = USB_DESCRIPTOR_ENDPOINT;
    endpoint_desc.endpoint_address = 0;
    endpoint_desc.attributes = 0;
    endpoint_desc.max_packet_size = device->desc.max_packet_size_0;
    endpoint_desc.interval = 0;

    // set up a new control pipe
    err = create_usb_pipe(hc, NULL, &pipe, &endpoint_desc, new_address, device->control_pipe->lowspeed);
    if (err < 0)
        goto err;

    // send the address change request
    err = send_usb_request(device, USB_REQTYPE_DEVICE_OUT, USB_REQUEST_SET_ADDRESS,
                           new_address, 0, 0, NULL);
    if (err < 0)
        goto err;

    // set the device's control pipe to the new one we just created
    device->control_pipe = pipe;

    device->address = new_address;

    return NO_ERROR;

err:
    free_usb_address(hc, new_address);
    return err;
}

static int load_device_configs(usb_device *device)
{
    int i;
    int err;

    for (i = 0; i < device->desc.num_configurations; i++) {
        usb_configuration *config;
        char buf[128];
        usb_configuration_descriptor *desc;
        char *buf2;
        void *ptr;
        usb_interface *last_non_alternate_interface = NULL;
        usb_interface *last_interface = NULL;

        // for now, make sure the config descriptor we have doesn't cross a page boundary
        desc = (usb_configuration_descriptor *)((((addr_t)buf) & 0xffffffc0) + 0x40);

        // create a structure for the configuration
        config = kmalloc(sizeof(usb_configuration));
        if (!config) {
            err = ERR_NO_MEMORY;
            goto err1;
        }
        memset(config, 0, sizeof(usb_configuration));

        // add it to the device
        config->dev = device;
        config->next = device->configs;
        device->configs = config;

        // load the config to see how much buffer we need to load the full one
        err = get_usb_descriptor(device, USB_DESCRIPTOR_CONFIGURATION, i, 0,
                                 desc, sizeof(usb_configuration_descriptor));
        if (err < 0) {
            goto err1;
        }

        // allocate a buffer to hold the full descriptor
        buf2 = kmalloc(desc->total_length);
        if (!buf2) {
            err = ERR_NO_MEMORY;
            goto err1;
        }

        // load the full configuration
        // this will load a buffer full of packed structures for each interface and endpoint
        err = get_usb_descriptor(device, USB_DESCRIPTOR_CONFIGURATION, i, 0,
                                 buf2, desc->total_length);
        if (err < 0) {
            kfree(buf2);
            goto err1;
        }
        config->desc = (usb_configuration_descriptor *)buf2;

        // now iterate through the rest of the buffer we just read
        ptr = (void *)((addr_t)buf2 + config->desc->length);
        while ((addr_t)ptr < ((addr_t)buf2 + config->desc->total_length)) {
            usb_descriptor *d = ptr;
            ptr = (void *)((addr_t)ptr + d->generic.length);

            SHOW_FLOW(5, "desc at %p: length %d type 0x%x", d, d->generic.length, d->generic.descriptor_type);

            switch (d->generic.descriptor_type) {
                case USB_DESCRIPTOR_INTERFACE: {
                    SHOW_FLOW(6, "interface: num %d alt setting %d num ep %d class %d subclass %d prot %d interface %d",
                              d->interface.interface_number, d->interface.alternate_setting,
                              d->interface.num_endpoints, d->interface.interface_class,
                              d->interface.interface_subclass, d->interface.interface_protocol,
                              d->interface.interface);

                    // allocate a new interface
                    usb_interface *i = kmalloc(sizeof(usb_interface));
                    if (!i) {
                        err = ERR_NO_MEMORY;
                        goto err1;
                    }

                    // set it up
                    i->desc = (usb_interface_descriptor *)&d->interface;
                    i->endpoints = NULL;
                    i->active_interface = NULL;

                    if (i->desc->alternate_setting == 0) {
                        i->next = config->interfaces;
                        config->interfaces = i;
                        last_non_alternate_interface = i;
                    } else {
                        // we're an alternate interface for the last interface
                        i->next = last_non_alternate_interface->alternates;
                        last_non_alternate_interface->alternates = i;
                    }

                    last_interface = i;
                    break;
                }
                case USB_DESCRIPTOR_ENDPOINT: {
                    SHOW_FLOW(6, "endpoint: addr %d attributes 0x%x, max packet size %d interval %d",
                              d->endpoint.endpoint_address, d->endpoint.attributes,
                              d->endpoint.max_packet_size, d->endpoint.interval);

                    // allocate a new endpoint
                    usb_endpoint *e = kmalloc(sizeof(usb_endpoint));
                    if (!e) {
                        err = ERR_NO_MEMORY;
                        goto err1;
                    }

                    // set it up
                    e->desc = (usb_endpoint_descriptor *)&d->endpoint;
                    e->pipe = NULL;
                    e->next = last_interface->endpoints;
                    last_interface->endpoints = e;
                }
            }
        }
    }

    return NO_ERROR;

err1:
    // clear out any loaded configs
    while (device->configs) {
        usb_configuration *config = device->configs;
        device->configs = config->next;

        while (config->interfaces) {
            usb_interface *interface = config->interfaces;
            config->interfaces = interface->next;

            while (interface->endpoints) {
                usb_endpoint *endpoint = interface->endpoints;
                interface->endpoints = endpoint->next;

                kfree(endpoint);
            }

            while (interface->alternates) {
                usb_interface *interface2 = config->interfaces;
                config->interfaces = interface2->next;

                while (interface2->endpoints) {
                    usb_endpoint *endpoint = interface2->endpoints;
                    interface2->endpoints = endpoint->next;

                    kfree(endpoint);
                }
            }

            kfree(interface);
        }

        if (config->desc)
            kfree(config->desc);
        kfree(config);
    }
err:
    return err;
}

int create_usb_interface(usb_device *device, usb_interface *interface)
{
    int err;
    usb_endpoint *e;

    // create a pipe on each endpoint
    for (e = interface->endpoints; e; e = e->next) {
        err = create_usb_pipe(device->hc, interface, &e->pipe, e->desc, device->address, device->control_pipe->lowspeed);
        if (err < 0) {
            // XXX deal
            return err;
        }
    }

    return NO_ERROR;
}

int set_usb_configuration(usb_device *device, usb_configuration *config)
{
    usb_interface *interface;
    int err;

    ASSERT(config->dev == device);

    if (device->active_config == config)
        return NO_ERROR;

    if (device->active_config) {
        // XXX tear down old configuration
        panic("set_usb_configuration: cannot tear down old configuration!\n");
        return ERR_UNIMPLEMENTED;
    }

    for (interface = config->interfaces; interface; interface = interface->next) {
        // if we had previously not set an active interface, use the primary one
        if (interface->active_interface == NULL)
            interface->active_interface = interface;

        err = create_usb_interface(device, interface->active_interface);
        if (err < 0) {
            // XXX deal with this
            return err;
        }
    }

    // send the set configuration command
    err = send_usb_request(device, USB_REQTYPE_DEVICE_OUT, USB_REQUEST_SET_CONFIGURATION,
                           config->desc->configuration_value, 0, 0, NULL);
    if (err < 0) {
        // XXX deal with this
        return err;
    }

    // send the set interface command for each interface that has alternate interfaces
    for (interface = config->interfaces; interface; interface = interface->next) {
        if (interface->desc->alternate_setting != 0) {
            err = send_usb_request(device, USB_REQTYPE_DEVICE_OUT, USB_REQUEST_SET_INTERFACE,
                                   interface->active_interface->desc->interface_number, 0, 0, NULL);
            if (err < 0) {
                // XXX deal with this
                return err;
            }
        }
    }

    device->active_config = config;

    return NO_ERROR;
}
