/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _LOOPBACK_H
#define _LOOPBACK_H

#include <kernel/net/if.h>
#include <kernel/cbuf.h>

// not to be called directly, use the ifnet.link_output and link_input
int loopback_input(cbuf *buf, ifnet *i);
int loopback_output(cbuf *buf, ifnet *i, netaddr *target, int protocol_type);

int loopback_init(void);

#endif

