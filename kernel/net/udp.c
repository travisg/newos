/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/cbuf.h>
#include <kernel/debug.h>
#include <kernel/net/udp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/misc.h>

typedef struct udp_header {
	uint16 source_port;
	uint16 dest_port;
	uint16 length;
	uint16 checksum;
} _PACKED udp_header;

typedef struct udp_pseudo_header {
	ipv4_addr source_addr;
	ipv4_addr dest_addr;
	uint8 zero;
	uint8 protocol;
	uint16 udp_length;
} _PACKED udp_pseudo_header;

int udp_receive(cbuf *buf, ifnet *i, ipv4_addr source_address, ipv4_addr target_address)
{
	udp_header *header;
	int err;

	header = cbuf_get_ptr(buf, 0);

	dprintf("udp_receive: src port %d, dest port %d, len %d, buf len %d, checksum 0x%x\n",
		ntohs(header->source_port), ntohs(header->dest_port), ntohs(header->length), cbuf_get_len(buf), header->checksum);

	if(ntohs(header->length) > (uint16)cbuf_get_len(buf)) {
		err = ERR_NET_BAD_PACKET;
		goto ditch_packet;
	}

	// deal with the checksum check
	if(header->checksum) {
		udp_pseudo_header pheader;
		uint16 checksum;

		// set up the pseudo header for checksum purposes
		pheader.source_addr = htonl(source_address);
		pheader.dest_addr = htonl(target_address);
		pheader.zero = 0;
		pheader.protocol = IP_PROT_UDP;
		pheader.udp_length = header->length;

		if(ntohs(header->length) % 2) {
			// make sure the pad byte is zero
			((uint8 *)header)[ntohs(header->length)] = 0;
		}
		checksum = cksum16_2(&pheader, sizeof(pheader), header, ROUNDUP(ntohs(header->length), 2));
		if(checksum != 0) {
			dprintf("udp_receive: packet failed checksum\n");
			err = ERR_NET_BAD_PACKET;
			goto ditch_packet;
		}
	}

	// XXX finish here


	err = NO_ERROR;

ditch_packet:
	cbuf_free_chain(buf);

	return err;
}

