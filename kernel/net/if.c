/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/khash.h>
#include <kernel/heap.h>
#include <kernel/cbuf.h>
#include <kernel/vfs.h>
#include <kernel/thread.h>
#include <kernel/arch/cpu.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/if.h>
#include <libc/string.h>

static void *iflist;
static if_id next_id;

static int if_compare_func(void *_i, void *_key)
{
	struct ifnet *i = _i;
	if_id *id = _key;

	if(i->id == *id) return 0;
	else return 1;
}

static unsigned int if_hash_func(void *_i, void *_key, int range)
{
	struct ifnet *i = _i;
	if_id *id = _key;

	if(i)
		return (i->id % range);
	else
		return (*id % range);
}

ifnet *if_register_interface(const char *path, int type)
{
	ifnet *i;

	i = kmalloc(sizeof(ifnet));
	if(!i)
		return NULL;

	i->id = atomic_add(&next_id, 1);
	strcpy(i->path, path);
	i->type = type;
	i->addr_list = NULL;
	i->link_addr = NULL;
	i->fd = -1;
	i->rx_thread = -1;

	return i;
}

void if_bind_address(ifnet *i, ifaddr *addr)
{
	addr->if_owner = i;
	addr->next = i->addr_list;
	i->addr_list = addr;
}

void if_bind_link_address(ifnet *i, ifaddr *addr)
{
	i->link_addr = addr;
}

int if_output(cbuf *b, ifnet *i)
{
	uint8 buf[2048];
	size_t len = cbuf_get_len(b);

	// put the cbuf into a contiguous buffer
	cbuf_memcpy_from_chain(buf, b, 0, len);

	// free the cbuf
	cbuf_free_chain(b);

	return sys_write(i->fd, buf, 0, len);
}

int if_rx_thread(void *args)
{
	ifnet *i = args;
	int err;
	cbuf *b;

	// open the network device
	i->fd = sys_open(i->path, STREAM_TYPE_DEVICE, 0);
	if(i->fd < 0) {
		dprintf("if_rx_thread: no net devices\n");
		return -1;
	}

	for(;;) {
		ssize_t len;

		len = sys_read(i->fd, i->rx_buf, 0, sizeof(i->rx_buf));
		dprintf("if_rx_thread: got ethernet packet, size %d\n", len);
		if(len < 0) {
			thread_snooze(10000);
			continue;
		}
		if(len == 0)
			continue;

		// check to see if we have a link layer address attached to us
		if(!i->link_addr)
			continue;

		// for now just move it over into a cbuf
		b = cbuf_get_chain(len);
		if(!b) {
			dprintf("if_rx_thread: could not allocate cbuf to hold ethernet packet\n");
			continue;
		}
		cbuf_memcpy_to_chain(b, 0, i->rx_buf, len);

		ethernet_receive(b, i);
	}

	return 0;
}

int if_init(void)
{
	ifnet *i;

	next_id = 0;

	// create a hash table to store the interface list
	iflist = hash_init(16, (addr)&i->next - (addr)i,
		&if_compare_func, &if_hash_func);

	return 0;
}

