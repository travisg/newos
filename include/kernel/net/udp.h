/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _UDP_H
#define _UDP_H

#include <kernel/net/if.h>
#include <kernel/cbuf.h>

int udp_receive(cbuf *buf, ifnet *i, ifaddr *target_addr);

#endif

