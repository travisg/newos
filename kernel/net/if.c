/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/khash.h>
#include <kernel/heap.h>
#include <kernel/cbuf.h>
#include <kernel/lock.h>
#include <kernel/sem.h>
#include <kernel/vfs.h>
#include <kernel/thread.h>
#include <kernel/queue.h>
#include <kernel/arch/cpu.h>
#include <kernel/net/loopback.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/if.h>
#include <libc/string.h>

#define TX_QUEUE_SIZE 64

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

ifnet *if_id_to_ifnet(if_id id)
{
	return hash_lookup(iflist, &id);
}

ifnet *if_register_interface(const char *path, int type)
{
	ifnet *i;

	i = kmalloc(sizeof(ifnet));
	if(!i)
		return NULL;

	// find the appropriate function calls to the link layer drivers
	switch(type) {
		case IF_TYPE_LOOPBACK:
			i->link_input = &loopback_input;
			i->link_output = &loopback_output;
			break;
		case IF_TYPE_ETHERNET:
			i->link_input = &ethernet_input;
			i->link_output = &ethernet_output;
			break;
		default:
			kfree(i);
			return NULL;
	}
	i->id = atomic_add(&next_id, 1);
	strcpy(i->path, path);
	i->type = type;
	i->addr_list = NULL;
	i->link_addr = NULL;
	i->fd = -1;
	i->rx_thread = -1;
	i->tx_thread = -1;
	i->tx_queue_sem = sem_create(0, "tx_queue_sem");
	mutex_init(&i->tx_queue_lock, "tx_queue_lock");
	fixed_queue_init(&i->tx_queue, TX_QUEUE_SIZE);

	hash_insert(iflist, i);

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
	bool release_sem = false;

	// stick the buffer on a transmit queue
	mutex_lock(&i->tx_queue_lock);
	fixed_queue_enqueue(&i->tx_queue, b);
	if(i->tx_queue.count == 1)
		release_sem = true;
	mutex_unlock(&i->tx_queue_lock);

	if(release_sem)
		sem_release(i->tx_queue_sem, 1);

	return NO_ERROR;
}

static int if_tx_thread(void *args)
{
	ifnet *i = args;
	cbuf *buf;
	int err;
	ssize_t len;

	if(i->fd < 0)
		return -1;

	for(;;) {
		sem_acquire(i->tx_queue_sem, 1);

		// pull a packet out of the queue
		mutex_lock(&i->tx_queue_lock);
		buf = fixed_queue_dequeue(&i->tx_queue);
		mutex_unlock(&i->tx_queue_lock);
		if(!buf)
			continue;

		// put the cbuf chain into a flat buffer
		len = cbuf_get_len(buf);
		cbuf_memcpy_from_chain(i->tx_buf, buf, 0, len);

		cbuf_free_chain(buf);

		sys_write(i->fd, i->tx_buf, 0, len);
	}
}

static int if_rx_thread(void *args)
{
	ifnet *i = args;
	int err;
	cbuf *b;

	if(i->fd < 0)
		return -1;

	for(;;) {
		ssize_t len;

		len = sys_read(i->fd, i->rx_buf, 0, sizeof(i->rx_buf));
		dprintf("if_rx_thread: got ethernet packet, size %ld\n", len);
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

		i->link_input(b, i);
	}

	return 0;
}

int if_boot_interface(ifnet *i)
{
	int err;

	i->fd = sys_open(i->path, STREAM_TYPE_DEVICE, 0);
	if(i->fd < 0)
		return i->fd;

	// create the receive thread
	i->rx_thread = thread_create_kernel_thread("net_rx_thread", &if_rx_thread, i);
	if(i->rx_thread < 0) {
		err = i->rx_thread;
		goto err1;
	}
	thread_set_priority(i->rx_thread, THREAD_HIGH_PRIORITY);

	// create the transmit thread
	i->tx_thread = thread_create_kernel_thread("net_tx_thread", &if_tx_thread, i);
	if(i->tx_thread < 0) {
		err = i->tx_thread;
		goto err2;
	}
	thread_set_priority(i->tx_thread, THREAD_HIGH_PRIORITY);

	// start the threads
	thread_resume_thread(i->rx_thread);
	thread_resume_thread(i->tx_thread);

	return NO_ERROR;

err2:
	thread_kill_thread_nowait(i->rx_thread);
err1:
	sys_close(i->fd);
err:
	return err;
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

