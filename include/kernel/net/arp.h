/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ARP_H
#define _ARP_H

#include <kernel/net/if.h>
#include <kernel/net/ipv4.h>
#include <kernel/cbuf.h>

int arp_receive(cbuf *buf, ifnet *i);
int arp_init(void);
int arp_lookup(ifnet *i, ipv4_addr sender_ipaddr, ipv4_addr ip_addr, ethernet_addr e_addr);
int arp_insert(ipv4_addr ip_addr, netaddr *link_addr);

#endif

