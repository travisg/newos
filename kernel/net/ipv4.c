/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/net/misc.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ipv4.h>

typedef struct ipv4_header {
	uint8 version_length;
	uint8 tos;
	uint16 total_length;
	uint16 identification;
	uint16 flags_frag_offfset;
	uint8 ttl;
	uint8 protocol;
	uint16 header_checksum;
	ipv4_addr src;
	ipv4_addr dest;
} _PACKED ipv4_header;

#define MIN_ARP_SIZE 28

typedef struct arp_packet {
	uint16 hard_type;
	uint16 prot_type;
	uint8  hard_size;
	uint8  prot_size;
	uint16 op;
	ethernet_addr sender_ethernet;
	ipv4_addr sender_ipv4;
	ethernet_addr target_ethernet;
	ipv4_addr target_ipv4;
} _PACKED arp_packet;

enum {
	ARP_OP_REQUEST = 1,
	ARP_OP_REPLY,
	ARP_OP_RARP_REQUEST,
	ARP_OP_RARP_REPLY
};

static ipv4_addr station_address = 0;

void dump_ipv4_addr(ipv4_addr addr)
{
	uint8 *nuaddr = (uint8 *)&addr;

	dprintf("%d.%d.%d.%d", nuaddr[0], nuaddr[1], nuaddr[2], nuaddr[3]);
}

static void dump_ipv4_header(ipv4_header *head)
{
	dprintf("ipv4 header: src ");
	dump_ipv4_addr(head->src);
	dprintf(" dest ");
	dump_ipv4_addr(head->dest);
	dprintf(" cksum 0x%x, len 0x%x\n",
		ntohs(head->header_checksum), ntohs(head->total_length));
}	

void ipv4_set_station_address(ipv4_addr address)
{
	station_address = ntohl(address);
	dprintf("ipv4_set_station_address: address set to ");
	dump_ipv4_addr(station_address);
	dprintf("\n");
}

int ipv4_receive(uint8 *buf, int offset, size_t len)
{
	ipv4_header *header;

	header = (ipv4_header *)(buf + offset);
	
	dump_ipv4_header(header);


	return 0;
}

static void dump_arp_packet(arp_packet *arp)
{
	dprintf("arp packet: src ");
	dump_ethernet_addr(arp->sender_ethernet);
	dprintf(" ");
	dump_ipv4_addr(arp->sender_ipv4);
	dprintf(" dest ");
	dump_ethernet_addr(arp->target_ethernet);
	dprintf(" ");
	dump_ipv4_addr(arp->target_ipv4);
	dprintf(" op 0x%x\n", ntohs(arp->op));
}

int arp_receive(uint8 *buf, int offset, size_t len)
{
	arp_packet *arp;

	arp = (arp_packet *)(buf + offset);

	if(len < MIN_ARP_SIZE)
		return -1;

	dump_arp_packet(arp);

	switch(ntohs(arp->op)) {
		case ARP_OP_REQUEST:
				
			break;
		default:
			dprintf("arp_receive: unhandled arp request type 0x%x\n", ntohs(arp->op));
	}

	return 0;
}


