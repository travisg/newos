/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _UDP_H
#define _UDP_H

#include <kernel/net/if.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/socket.h>
#include <kernel/cbuf.h>

int udp_input(cbuf *buf, ifnet *i, ipv4_addr source_address, ipv4_addr target_address);
int udp_open(netaddr *addr, uint16 port, void **prot_data);
int udp_close(void *prot_data);
ssize_t udp_recvfrom(void *prot_data, void *buf, ssize_t len, sockaddr *saddr);
ssize_t udp_sendto(void *prot_data, const void *buf, ssize_t len, sockaddr *addr);
int udp_init(void);

#endif

