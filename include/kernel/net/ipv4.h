/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _IPV4_H
#define _IPV4_H

#include <kernel/net/if.h>
#include <kernel/cbuf.h>

typedef uint32 ipv4_addr;

enum {
	IP_PROT_ICMP = 1,
	IP_PROT_UDP = 17,
};

#define NETADDR_TO_IPV4(naddr) (*(ipv4_addr *)&(naddr)->addr[0])
#define IPV4_DOTADDR_TO_ADDR(a, b, c, d) \
	(((ipv4_addr)(a) << 24) | (((ipv4_addr)(b) & 0xff) << 16) | (((ipv4_addr)(c) & 0xff) << 8) | ((ipv4_addr)(d) & 0xff))

int ipv4_route_add(ipv4_addr network_addr, ipv4_addr netmask, ipv4_addr if_addr, if_id interface_num);
int ipv4_route_add_gateway(ipv4_addr network_addr, ipv4_addr netmask, ipv4_addr if_addr, if_id interface_num, ipv4_addr gw_addr);

int ipv4_lookup_srcaddr_for_dest(ipv4_addr dest_addr, ipv4_addr *src_addr);

int ipv4_input(cbuf *buf, ifnet *i);
int ipv4_output(cbuf *buf, ipv4_addr target_addr, int protocol);
int ipv4_init(void);

void dump_ipv4_addr(ipv4_addr addr);

#endif

