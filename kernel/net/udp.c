/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/cbuf.h>
#include <kernel/debug.h>
#include <kernel/net/udp.h>

typedef struct udp_header {
	uint16 source_port;
	uint16 dest_port;
	uint16 length;
	uint16 checksum;
} _PACKED udp_header;

int udp_receive(cbuf *buf, ifnet *i, ifaddr *target_addr)
{
	cbuf_free_chain(buf);

	return 0;
}

