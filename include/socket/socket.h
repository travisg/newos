/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_INCLUDE_SOCKET_SOCKET_H
#define _NEWOS_INCLUDE_SOCKET_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <newos/types.h>
#include <newos/socket_api.h>

int socket_create(int proto, int flags);
int socket_close(int fd);
int socket_bind(int fd, sockaddr *addr);
int socket_connect(int fd, sockaddr *addr);
ssize_t socket_read(int fd, void *buf, ssize_t len);
ssize_t socket_write(int fd, const void *buf, ssize_t len);
ssize_t socket_recvfrom(int fd, void *buf, ssize_t len, sockaddr *addr);
ssize_t socket_recvfrom_etc(int fd, void *buf, ssize_t len, sockaddr *addr, int flags, bigtime_t timeout);
ssize_t socket_sendto(int fd, const void *buf, ssize_t len, sockaddr *addr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

