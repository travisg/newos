/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ETHERNET_H
#define _ETHERNET_H

#include <kernel/net/if.h>
#include <kernel/cbuf.h>

#define PROT_TYPE_IPV4 0x0800
#define PROT_TYPE_ARP  0x0806

typedef uint8 ethernet_addr[6];

int ethernet_input(cbuf *buf, ifnet *i);
int ethernet_output(cbuf *buf, ifnet *i, ethernet_addr target, int protocol_type);
int ethernet_broadcast_output(cbuf *buf, ifnet *i, int protocol_type);
int ethernet_init(void);


void dump_ethernet_addr(ethernet_addr addr);

#endif

