/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/lock.h>
#include <kernel/heap.h>
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

#define IPV4_FLAG_MORE_FRAGS   0x2000
#define IPV4_FLAG_MAY_NOT_FRAG 0x4000
#define IPV4_FRAG_OFFSET_MASK  0x1fff

typedef struct ipv4_routing_entry {
	struct ipv4_routing_entry *next;
	ipv4_addr network_addr;
	ipv4_addr netmask;
	ipv4_addr gw_addr;
	ipv4_addr if_addr;
	if_id interface_id;
	int flags;
} ipv4_routing_entry;

#define ROUTE_FLAGS_GW 1

static ipv4_routing_entry *route_table;
static mutex route_table_mutex;

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

static int ipv4_route_add_etc(ipv4_addr network_addr, ipv4_addr netmask, ipv4_addr if_addr, if_id interface_num, int flags, ipv4_addr gw_addr)
{
	ipv4_routing_entry *e;
	ipv4_routing_entry *temp;
	ipv4_routing_entry *last;

	// make sure the netmask makes sense
	if((netmask | (netmask - 1)) != 0xffffffff) {
		return ERR_INVALID_ARGS;
	}

	e = kmalloc(sizeof(ipv4_routing_entry));
	if(!e)
		return ERR_NO_MEMORY;

	e->network_addr = network_addr;
	e->netmask = netmask;
	e->gw_addr = gw_addr;
	e->if_addr = if_addr;
	e->interface_id = interface_num;
	e->flags = flags;

	mutex_lock(&route_table_mutex);

	// add it to the list, sorted by netmask 'completeness'
	last = NULL;
	for(temp = route_table; temp; temp = temp->next) {
		if((netmask | e->netmask) == e->netmask) {
			// insert our route entry here
			break;
		}
		last = temp;
	}
	if(last)
		last->next = e;
	else
		route_table = e;
	e->next = temp;

	mutex_unlock(&route_table_mutex);

	return NO_ERROR;
}

int ipv4_route_add(ipv4_addr network_addr, ipv4_addr netmask, ipv4_addr if_addr, if_id interface_num)
{
	return ipv4_route_add_etc(network_addr, netmask, if_addr, interface_num, 0, 0);
}

int ipv4_route_add_gateway(ipv4_addr network_addr, ipv4_addr netmask, ipv4_addr if_addr, if_id interface_num, ipv4_addr gw_addr)
{
	return ipv4_route_add_etc(network_addr, netmask, if_addr, interface_num, ROUTE_FLAGS_GW, gw_addr);
}

static int ipv4_route_match(ipv4_addr ip_addr, if_id *interface_num, ipv4_addr *target_addr, ipv4_addr *if_addr)
{
	ipv4_routing_entry *e;
	ipv4_routing_entry *last_e = NULL;
	int err;

	// walk through the routing table, finding the last entry to match
	mutex_lock(&route_table_mutex);
	for(e = route_table; e; e = e->next) {
		ipv4_addr masked_addr = ip_addr & e->netmask;
		if(masked_addr == e->network_addr)
			last_e = e;
	}

	if(last_e) {
		*interface_num = last_e->interface_id;
		*if_addr = last_e->if_addr;
		if(last_e->flags & ROUTE_FLAGS_GW) {
			*target_addr = last_e->gw_addr;
		} else {
			*target_addr = ip_addr;
		}
		err = NO_ERROR;
	} else {
		*interface_num = -1;
		*target_addr = 0;
		*if_addr = 0;
		err = ERR_NET_GENERAL;
	}
	mutex_unlock(&route_table_mutex);

	return err;
}

static void ipv4_arp_callback(int arp_code, void *args, ifnet *i, netaddr *link_addr)
{
	cbuf *buf = args;

	if(arp_code == ARP_CALLBACK_CODE_OK) {
		// arp found us an address and called us back with it
		i->link_output(buf, i, link_addr, PROT_TYPE_IPV4);
	} else if(arp_code == ARP_CALLBACK_CODE_FAILED) {
		// arp retransmitted and failed, so we're screwed
		cbuf_free_chain(buf);
	} else {
		// uh
		;
	}
}

int ipv4_output(cbuf *buf, ipv4_addr target_addr, int protocol)
{
	cbuf *header_buf;
	ipv4_header *header;
	netaddr link_addr;
	if_id iid;
	ifnet *i;
	ipv4_addr transmit_addr;
	ipv4_addr if_addr;
	int err;

	dprintf("ipv4_output: buf %p, target_addr ", buf);
	dump_ipv4_addr(target_addr);
	dprintf(", protocol %d\n", protocol);

	// figure out what interface we will send this over
	err = ipv4_route_match(target_addr, &iid, &transmit_addr, &if_addr);
	if(err < 0) {
		cbuf_free_chain(buf);
		return ERR_NO_MEMORY;
	}
	i = if_id_to_ifnet(iid);
	if(!i) {
		cbuf_free_chain(buf);
		return ERR_NO_MEMORY;
	}

//	dprintf("did route match, result iid %d, i 0x%x, transmit_addr 0x%x, if_addr 0x%x\n", iid, i, transmit_addr, if_addr);

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
	err = arp_lookup(i, if_addr, transmit_addr, &link_addr, &ipv4_arp_callback, buf);
	if(err == ERR_NET_ARP_QUEUED) {
		// the arp request is queued up so we can just exit here
		// and the rest of the work will be done via the arp callback
		return NO_ERROR;
	} else if(err < 0) {
		dprintf("ipv4_output: failed arp lookup\n");
		cbuf_free_chain(buf);
		return ERR_NET_FAILED_ARP;
	} else {
		// we got the link later address, send the packet
		return i->link_output(buf, i, &link_addr, PROT_TYPE_IPV4);
	}
}

int ipv4_input(cbuf *buf, ifnet *i)
{
	int err;
	ipv4_header *header;

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
	if(ntohl(header->dest) != 0xffffffff) {
		ifaddr *iaddr;
		ipv4_addr dest = ntohl(header->dest);

		for(iaddr = i->addr_list; iaddr; iaddr = iaddr->next) {
			if(iaddr->addr.type == ADDR_TYPE_IP) {
				// see if it matches one of this interface's ip addresses
				if(dest == NETADDR_TO_IPV4(&iaddr->addr))
					break;
				// see if it matches the broadcast address
				if(dest == NETADDR_TO_IPV4(&iaddr->broadcast))
					break;
			}
		}
		if(!iaddr) {
			dprintf("ipv4 packet for someone else\n");
			err = NO_ERROR;
			goto ditch_packet;
		}
	}

	// see if it's a fragment
	if(ntohs(header->flags_frag_offset) & IPV4_FLAG_MORE_FRAGS ||
		(ntohs(header->flags_frag_offset) & IPV4_FRAG_OFFSET_MASK) != 0) {
		dprintf("ipv4 packet is fragmented, cannot handle that now\n");
		err = ERR_UNIMPLEMENTED;
		goto ditch_packet;
	}

	// strip off the ip header
	cbuf_truncate_head(buf, (header->version_length & 0xf) * 4);

	// demultiplex and hand to the proper module
	switch(header->protocol) {
		case IP_PROT_ICMP:
			return icmp_input(buf, i, ntohl(header->src));
		case IP_PROT_UDP:
			return udp_input(buf, i, ntohl(header->src), ntohl(header->dest));
		default:
			dprintf("ipv4_receive: packet with unknown protocol (%d)\n", header->protocol);
			err = ERR_NET_BAD_PACKET;
			goto ditch_packet;
	}

	err = NO_ERROR;

ditch_packet:
	cbuf_free_chain(buf);

	return err;
}

int ipv4_init(void)
{
	mutex_init(&route_table_mutex, "ipv4 routing table mutex");

	route_table = NULL;

	return 0;
}

