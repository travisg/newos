/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_NET_SOCKET_H
#define _NEWOS_KERNEL_NET_SOCKET_H

#include <kernel/net/net.h>

enum {
	SOCK_PROTO_NULL = 0,
	SOCK_PROTO_UDP,
	SOCK_PROTO_TCP
};

#define SOCK_FLAG_TIMEOUT 1

typedef int32 sock_id;

typedef struct sockaddr {
	netaddr addr;
	int port;
} sockaddr;

int socket_init(void);
sock_id socket_create(int type, int flags);
int socket_bind(sock_id id, sockaddr *addr);
int socket_connect(sock_id id, sockaddr *addr);
ssize_t socket_recvfrom(sock_id id, void *buf, ssize_t len, sockaddr *addr);
ssize_t socket_recvfrom_etc(sock_id id, void *buf, ssize_t len, sockaddr *addr, int flags, bigtime_t timeout);
ssize_t socket_sendto(sock_id id, const void *buf, ssize_t len, sockaddr *addr);

#endif

