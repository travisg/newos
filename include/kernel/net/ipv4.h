/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_NET_IPV4_H
#define _NEWOS_KERNEL_NET_IPV4_H

#include <kernel/net/if.h>
#include <kernel/cbuf.h>
#include <newos/net.h>

int ipv4_route_add(ipv4_addr network_addr, ipv4_addr netmask, ipv4_addr if_addr, if_id interface_num);
int ipv4_route_add_gateway(ipv4_addr network_addr, ipv4_addr netmask, ipv4_addr if_addr, if_id interface_num, ipv4_addr gw_addr);

int ipv4_lookup_srcaddr_for_dest(ipv4_addr dest_addr, ipv4_addr *src_addr);
int ipv4_get_mss_for_dest(ipv4_addr dest_addr, uint32 *mss);

int ipv4_input(cbuf *buf, ifnet *i);
int ipv4_output(cbuf *buf, ipv4_addr target_addr, int protocol);
int ipv4_init(void);

void dump_ipv4_addr(ipv4_addr addr);

#endif

