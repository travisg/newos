/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_NET_H
#define _NEWOS_NET_H

#include <newos/types.h>

/* contains common network stuff */

typedef struct netaddr {
	uint8 len;
	uint8 type;
	uint8 pad0;
	uint8 pad1;
	uint8 addr[12];
} netaddr;

enum {
	SOCK_PROTO_NULL = 0,
	SOCK_PROTO_UDP,
	SOCK_PROTO_TCP
};

enum {
	ADDR_TYPE_NULL = 0,
	ADDR_TYPE_ETHERNET,
	ADDR_TYPE_IP
};

#define SOCK_FLAG_TIMEOUT 1

typedef struct sockaddr {
	netaddr addr;
	int port;
} sockaddr;

enum {
	IP_PROT_ICMP = 1,
	IP_PROT_TCP = 6,
	IP_PROT_UDP = 17,
};

typedef uint32 ipv4_addr;
#define NETADDR_TO_IPV4(naddr) (*(ipv4_addr *)(&((&(naddr))->addr[0])))
#define IPV4_DOTADDR_TO_ADDR(a, b, c, d) \
	(((ipv4_addr)(a) << 24) | (((ipv4_addr)(b) & 0xff) << 16) | (((ipv4_addr)(c) & 0xff) << 8) | ((ipv4_addr)(d) & 0xff))


#endif
