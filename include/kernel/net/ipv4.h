/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _IPV4_H
#define _IPV4_H

// for the ethernet module
#define PROT_TYPE_IPV4 0x0800
#define PROT_TYPE_ARP  0x0806

typedef uint32 ipv4_addr;

void ipv4_set_station_address(ipv4_addr address);
int ipv4_receive(uint8 *buf, int offset, size_t len);
int arp_receive(uint8 *buf, int offset, size_t len);

void dump_ipv4_addr(ipv4_addr addr);

#endif

