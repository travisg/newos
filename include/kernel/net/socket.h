/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NET_SOCKET_H
#define _NET_SOCKET_H

#include <kernel/net/net.h>

enum {
	SOCK_PROTO_NULL = 0,
	SOCK_PROTO_UDP
};

typedef int32 sock_id;

typedef struct sockaddr {
	netaddr addr;
	int port;
} sockaddr;

int socket_init(void);
sock_id socket_create(int type, int flags, sockaddr *addr);
ssize_t socket_read(sock_id id, void *buf, ssize_t len);

#endif

