/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_NET_SOCKET_H
#define _NEWOS_KERNEL_NET_SOCKET_H

#include <kernel/net/net.h>
#include <newos/net.h>

typedef int32 sock_id;

int socket_init(void);
sock_id socket_create(int type, int flags);
int socket_bind(sock_id id, sockaddr *addr);
int socket_connect(sock_id id, sockaddr *addr);
ssize_t socket_read(sock_id id, void *buf, ssize_t len);
ssize_t socket_write(sock_id id, const void *buf, ssize_t len);
ssize_t socket_recvfrom(sock_id id, void *buf, ssize_t len, sockaddr *addr);
ssize_t socket_recvfrom_etc(sock_id id, void *buf, ssize_t len, sockaddr *addr, int flags, bigtime_t timeout);
ssize_t socket_sendto(sock_id id, const void *buf, ssize_t len, sockaddr *addr);
int socket_close(sock_id id);

int socket_dev_init(void);

#endif

