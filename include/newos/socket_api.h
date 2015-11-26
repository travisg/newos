/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_SOCKET_API_H
#define _NEWOS_SOCKET_API_H

#include <newos/types.h>
#include <newos/net.h>

typedef struct _socket_api_transfer_t {
    void *buf;
    ssize_t len;
    int flags;
    sockaddr *saddr;
    bigtime_t timeout;
} _socket_api_transfer_t;

typedef struct _socket_api_connect_t {
    sockaddr *saddr;
} _socket_api_connect_t;

typedef struct _socket_api_assoc_socket_t {
    int id;
} _socket_api_assoc_socket_t;

typedef struct _socket_api_accept_t {
    sockaddr *saddr;
} _socket_api_accept_t;

typedef struct _socket_api_bind_t {
    sockaddr *saddr;
} _socket_api_bind_t;

typedef struct _socket_api_create_t {
    int type;
    int flags;
} _socket_api_create_t;

typedef struct _socket_api_args_t {
    union {
        _socket_api_transfer_t transfer;
        _socket_api_connect_t connect;
        _socket_api_assoc_socket_t associate;
        _socket_api_accept_t accept;
        _socket_api_bind_t bind;
        _socket_api_create_t create;
    } u;
} _socket_api_args_t;

enum {
    _SOCKET_API_CREATE,
    _SOCKET_API_ASSOCIATE_SOCKET,
    _SOCKET_API_BIND,
    _SOCKET_API_CONNECT,
    _SOCKET_API_LISTEN,
    _SOCKET_API_ACCEPT,
    _SOCKET_API_RECVFROM,
    _SOCKET_API_RECVFROM_ETC,
    _SOCKET_API_SENDTO,
};

#endif
