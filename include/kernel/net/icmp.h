/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ICMP_H
#define _ICMP_H

#include <kernel/net/if.h>
#include <kernel/net/ipv4.h>
#include <kernel/cbuf.h>

int icmp_input(cbuf *buf, ifnet *i, ipv4_addr source_ipaddr);

#endif

