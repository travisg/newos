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

int ipv4_receive(cbuf *buf, ifnet *i);
int ipv4_output(cbuf *buf, ifnet *i, ipv4_addr target_addr, ipv4_addr if_addr, int protocol);

void dump_ipv4_addr(ipv4_addr addr);

#endif

