/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/ktypes.h>
#include <kernel/net/misc.h>
#include <libc/string.h>

uint16 cksum16(void *_buf, int len)
{
	uint32 sum = 0;
	uint16 *buf = _buf;

	while(len > 1) {
		sum += *buf++;
		if(sum & 0x80000000)
			sum = (sum % 0xffff) + (sum >> 16);
		len -= 2;
	}

	if(len)
		sum += (uint16)*(uint8 *)buf;

	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

uint16 cksum16_2(void *buf1, int len1, void *buf2, int len2)
{
	uint32 sum = 0;
	uint16 *buf = buf1;

	while(len1 > 1) {
		sum += *buf++;
		if(sum & 0x80000000)
			sum = (sum % 0xffff) + (sum >> 16);
		len1 -= 2;
	}

	if(len1)
		sum += (uint16)*(uint8 *)buf;

	buf = buf2;

	while(len2 > 1) {
		sum += *buf++;
		if(sum & 0x80000000)
			sum = (sum % 0xffff) + (sum >> 16);
		len2 -= 2;
	}

	if(len2)
		sum += (uint16)*(uint8 *)buf;

	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

int cmp_netaddr(netaddr *addr1, netaddr *addr2)
{
	if(addr1->type != addr2->type)
		return -1;
	return memcmp(&addr1->addr[0], &addr2->addr[0], addr1->len);
}

