/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_NET_ICMP_H
#define _NEWOS_KERNEL_NET_ICMP_H

#include <kernel/net/if.h>
#include <kernel/net/ipv4.h>
#include <kernel/cbuf.h>

int icmp_input(cbuf *buf, ifnet *i, ipv4_addr source_ipaddr);

#endif

