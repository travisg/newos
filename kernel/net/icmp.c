/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/cbuf.h>
#include <kernel/debug.h>
#include <kernel/net/icmp.h>
#include <kernel/net/misc.h>

typedef struct icmp_header {
	uint8 type;
	uint8 code;
	uint16 checksum;
} _PACKED icmp_header;

typedef struct icmp_echo_header {
	icmp_header preheader;
	uint16 identifier;
	uint16 sequence;
} _PACKED icmp_echo_header;

int icmp_receive(cbuf *buf, ifnet *i, ipv4_addr source_ipaddr)
{
	icmp_header *header;
	int err;

	header = (icmp_header *)cbuf_get_ptr(buf, 0);

	dprintf("icmp_message: header type %d, code %d, checksum 0x%x, length %d\n", header->type, header->code, header->checksum, cbuf_get_len(buf));

	// calculate the checksum on the whole thing
	if(cksum16(header, cbuf_get_len(buf)) != 0) {
		dprintf("icmp message fails cksum\n");
		err = ERR_NET_BAD_PACKET;
		goto ditch_message;
	}

	switch(header->type) {
		case 0: { // echo reply
			icmp_echo_header *eheader = (icmp_echo_header *)header;

			break;
		}
		case 8: { // echo request
			icmp_echo_header *eheader = (icmp_echo_header *)header;

			// bounce this message right back
			eheader->preheader.type = 0; // echo reply
			eheader->preheader.checksum = 0;
			eheader->preheader.checksum = cksum16(eheader, cbuf_get_len(buf));
			return ipv4_output(buf, source_ipaddr, IP_PROT_ICMP);
		}
		default:
			dprintf("unhandled icmp message\n");
	}

	err = NO_ERROR;

ditch_message:
	cbuf_free_chain(buf);

	return err;
}

