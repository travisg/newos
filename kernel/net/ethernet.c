/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/cbuf.h>
#include <kernel/net/misc.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/arp.h>
#include <libc/string.h>

#define MIN_ETHERNET2_LEN 46

typedef struct ethernet2_header {
	ethernet_addr dest;
	ethernet_addr src;
	uint16 type;
} ethernet2_header;

static ethernet_addr broadcast_ethernet;

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

int ethernet_input(cbuf *buf, ifnet *i)
{
	int err;
	ethernet2_header *e2_head;

	if(cbuf_get_len(buf) < MIN_ETHERNET2_LEN)
		return -1;

	e2_head = cbuf_get_ptr(buf, 0);

	dump_ethernet_header(e2_head);

	// strip out the ethernet header
	cbuf_truncate_head(buf, sizeof(ethernet2_header));
	// strip off the crc
	cbuf_truncate_tail(buf, 4);

	switch(ntohs(e2_head->type)) {
		case PROT_TYPE_IPV4:
			err = ipv4_input(buf, i);
			break;
		case PROT_TYPE_ARP:
			err = arp_input(buf, i);
			break;
		default:
			dprintf("ethernet_receive: unknown ethernet type 0x%x\n", ntohs(e2_head->type));
			err = -1;
	}

	return err;
}

int ethernet_broadcast_output(cbuf *buf, ifnet *i, int protocol_type)
{
	return ethernet_output(buf, i, broadcast_ethernet, protocol_type);
}

int ethernet_output(cbuf *buf, ifnet *i, ethernet_addr target, int protocol_type)
{
	cbuf *eheader_buf;
	ethernet2_header *eheader;

	eheader_buf = cbuf_get_chain(sizeof(ethernet2_header));
	if(!eheader_buf) {
		dprintf("ethernet_output: error allocating cbuf for eheader\n");
		cbuf_free_chain(buf);
		return ERR_NO_MEMORY;
	}

	// put together an ethernet header
	eheader = (ethernet2_header *)cbuf_get_ptr(eheader_buf, 0);
	memcpy(&eheader->dest, target, 6);
	memcpy(&eheader->src, &i->link_addr->addr.addr[0], 6);
	eheader->type = htons(protocol_type);

	// chain the buffers together
	buf = cbuf_merge_chains(eheader_buf, buf);

	return if_output(buf, i);
}

int ethernet_init(void)
{
	memset(broadcast_ethernet, 0xff, 6);

	return 0;
}

