/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/arch/cpu.h>
#include <kernel/net/misc.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/icmp.h>
#include <kernel/net/udp.h>
#include <kernel/net/arp.h>
#include <libc/string.h>

typedef struct ipv4_header {
	uint8 version_length;
	uint8 tos;
	uint16 total_length;
	uint16 identification;
	uint16 flags_frag_offset;
	uint8 ttl;
	uint8 protocol;
	uint16 header_checksum;
	ipv4_addr src;
	ipv4_addr dest;
} _PACKED ipv4_header;

static uint32 curr_identification = 0xbeef;

// expects hosts order
void dump_ipv4_addr(ipv4_addr addr)
{
	uint8 *nuaddr = (uint8 *)&addr;

	dprintf("%d.%d.%d.%d", nuaddr[3], nuaddr[2], nuaddr[1], nuaddr[0]);
}

static void dump_ipv4_header(ipv4_header *head)
{
	dprintf("ipv4 header: src ");
	dump_ipv4_addr(ntohl(head->src));
	dprintf(" dest ");
	dump_ipv4_addr(ntohl(head->dest));
	dprintf(" prot %d, cksum 0x%x, len 0x%x, ident 0x%x, frag offset 0x%x\n",
		head->protocol, ntohs(head->header_checksum), ntohs(head->total_length), ntohs(head->identification), ntohs(head->flags_frag_offset & 0x1fff));
}

int ipv4_output(cbuf *buf, ifnet *i, ipv4_addr target_addr, ipv4_addr if_addr, int protocol)
{
	cbuf *header_buf;
	ipv4_header *header;
	ethernet_addr eaddr;

	header_buf = cbuf_get_chain(sizeof(ipv4_header));
	if(!header_buf) {
		cbuf_free_chain(buf);
		return ERR_NO_MEMORY;
	}
	header = cbuf_get_ptr(header_buf, 0);

	header->version_length = 0x4 << 4 | 5;
	header->tos = 0;
	header->total_length = htons(cbuf_get_len(buf) + cbuf_get_len(header_buf));
	header->identification = htons(atomic_add(&curr_identification, 1));
	header->flags_frag_offset = htons(0x2 << 13 | 0); // dont fragment bit
	header->ttl = 255;
	header->protocol = protocol;
	header->header_checksum = 0;
	header->src = htonl(if_addr);
	header->dest = htonl(target_addr);

	// calculate the checksum
	header->header_checksum = cksum16(header, (header->version_length & 0xf) * 4);

	buf = cbuf_merge_chains(header_buf, buf);

	// do the arp thang
	if(arp_lookup(i, if_addr, target_addr, eaddr) < 0) {
		dprintf("ipv4_output: failed arp lookup\n");
		cbuf_free_chain(buf);
		return ERR_NET_FAILED_ARP;
	}

	// send the packet
	return ethernet_output(buf, i, eaddr, PROT_TYPE_IPV4);
}

int ipv4_receive(cbuf *buf, ifnet *i)
{
	int err;
	ipv4_header *header;
	ifaddr *iaddr;
	ifaddr ifaddr;

	header = (ipv4_header *)cbuf_get_ptr(buf, 0);

	dump_ipv4_header(header);

	if(((header->version_length >> 4) & 0xf) != 4) {
		dprintf("ipv4 packet has bad version\n");
		err = ERR_NET_BAD_PACKET;
		goto ditch_packet;
	}

	if(cksum16(header, (header->version_length & 0xf) * 4) != 0) {
		dprintf("ipv4 packet failed cksum\n");
		err = ERR_NET_BAD_PACKET;
		goto ditch_packet;
	}

	// verify that this packet is for us
	for(iaddr = i->addr_list; iaddr; iaddr = iaddr->next) {
		if(iaddr->addr.type == ADDR_TYPE_IP) {
			if(ntohl(header->dest) == *(ipv4_addr *)&iaddr->addr.addr[0])
				break;
		}
	}
	if(!iaddr) {
		dprintf("ipv4 packet for someone else\n");
		err = NO_ERROR;
		goto ditch_packet;
	}
	memcpy(&ifaddr, iaddr, sizeof(ifaddr));

	// strip off the ip header
	cbuf_truncate_head(buf, (header->version_length & 0xf) * 4);

	// demultiplex and hand to the proper module
	switch(header->protocol) {
		case IP_PROT_ICMP:
			return icmp_receive(buf, i, ntohl(header->src), &ifaddr);
		case IP_PROT_UDP:
			return udp_receive(buf, i, &ifaddr);
		default:
			dprintf("ipv4_receive: packet with unknown protocol (%d)\n");
			err = ERR_NET_BAD_PACKET;
			goto ditch_packet;
	}

	err = NO_ERROR;

ditch_packet:
	cbuf_free_chain(buf);

	return err;
}

