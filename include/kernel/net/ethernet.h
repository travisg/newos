/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ETHERNET_H
#define _ETHERNET_H

typedef uint8 ethernet_addr[6];

void ethernet_set_station_addr(ethernet_addr address);
void ethernet_get_station_addr(ethernet_addr address);
int ethernet_receive(uint8 *buf, int offset, size_t len);

void dump_ethernet_addr(ethernet_addr addr);

#endif

