/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/net/misc.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ipv4.h>
#include <libc/string.h>

#define MIN_ETHERNET2_LEN 46

typedef struct ethernet2_header {
	ethernet_addr dest;
	ethernet_addr src;
	uint16 type;
} ethernet2_header;

static ethernet_addr station_address;

void dump_ethernet_addr(ethernet_addr addr)
{
	dprintf("%x:%x:%x:%x:%x:%x", 
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static void dump_ethernet_header(ethernet2_header *head)
{
	dprintf("ethernet 2 header: dest ");
	dump_ethernet_addr(head->dest);
	dprintf(" src ");
	dump_ethernet_addr(head->src);
	dprintf(" type 0x%x\n", ntohs(head->type));
}	
	
void ethernet_set_station_addr(ethernet_addr address)
{
	memcpy(station_address, address, sizeof(ethernet_addr));
	dprintf("ethernet_set_station_addr: station address is ");
	dump_ethernet_addr(station_address);
	dprintf("\n");
}

void ethernet_get_station_addr(ethernet_addr address)
{
	memcpy(address, station_address, sizeof(ethernet_addr));
}

int ethernet_receive(uint8 *buf, int offset, size_t len)
{
	int err;
	ethernet2_header *e2_head;

	if(len < MIN_ETHERNET2_LEN) 
		return -1;

	e2_head = (ethernet2_header *)(buf + offset);

	dump_ethernet_header(e2_head);
 
	switch(ntohs(e2_head->type)) {
		case PROT_TYPE_IPV4:
			err = ipv4_receive(buf, offset + sizeof(ethernet2_header), len - sizeof(ethernet2_header));
			break;
		case PROT_TYPE_ARP:
			err = arp_receive(buf, offset + sizeof(ethernet2_header), len - sizeof(ethernet2_header));
			break;
		default:
			dprintf("ethernet_receive: unknown ethernet type 0x%x\n", ntohs(e2_head->type));
			err = -1;
	}

	return err;
}


