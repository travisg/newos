/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/khash.h>
#include <kernel/lock.h>
#include <kernel/sem.h>
#include <kernel/heap.h>
#include <kernel/arch/cpu.h>
#include <kernel/net/misc.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/arp.h>
#include <libc/string.h>

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

enum {
	ARP_HARD_TYPE_ETHERNET = 1
};

typedef struct arp_cache_entry {
	struct arp_cache_entry *next;
	ipv4_addr ip_addr;
	netaddr link_addr;
	time_t last_used_time;
} arp_cache_entry;

// arp cache
static void *arp_table;
static mutex arp_table_mutex;

typedef struct arp_wait_request {
	struct arp_wait_request *prev;
	struct arp_wait_request *next;
	ipv4_addr ip_addr;
	sem_id blocking_sem;
	int attempt_count;
} arp_wait_request;

// list of threads blocked on arp requests
static arp_wait_request *arp_waiters;
static mutex arp_wait_mutex;

static int arp_cache_compare(void *_e, void *_key)
{
	arp_cache_entry *e = _e;
	ipv4_addr *addr = _key;

	if(e->ip_addr == *addr)
		return 0;
	else
		return 1;
}

static unsigned int arp_cache_hash(void *_e, void *_key, int range)
{
	arp_cache_entry *e = _e;
	ipv4_addr *key = _key;
	ipv4_addr *addr;

	if(e)
		addr = &e->ip_addr;
	else
		addr = key;

	// XXX make this smarter
	return ((*addr) ^ (*addr >> 8) ^ (*addr >> 16) ^ (*addr > 24)) % range;
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

int arp_receive(cbuf *buf, ifnet *i)
{
	arp_packet *arp;

	arp = (arp_packet *)cbuf_get_ptr(buf, 0);

	if(cbuf_get_len(buf) < MIN_ARP_SIZE)
		return -1;

	dump_arp_packet(arp);

	// right now we can only deal with ipv4 arps dealing with ethernet
	if(ntohs(arp->prot_type) != PROT_TYPE_IPV4)
		goto out;
	if(ntohs(arp->hard_type) != ARP_HARD_TYPE_ETHERNET)
		goto out;

	switch(ntohs(arp->op)) {
		case ARP_OP_REPLY: {
			netaddr eth;

			eth.len = 6;
			eth.type = ADDR_TYPE_ETHERNET;
			memcpy(&eth.addr[0], &arp->sender_ethernet, 6);
			arp_insert(ntohl(arp->sender_ipv4), &eth);
			break;
		}
		case ARP_OP_REQUEST: {
			// we have an arp request, see if it matches with this interface
			ifaddr *iaddr = NULL;
			ifaddr *addr;

			// insert an arp entry for the sender of the arp, regardless of if it's
			// for us or not.
			{
				netaddr eth;

				eth.len = 6;
				eth.type = ADDR_TYPE_ETHERNET;
				memcpy(&eth.addr[0], &arp->sender_ethernet, 6);
				arp_insert(ntohl(arp->sender_ipv4), &eth);
			}

			for(addr = i->addr_list; addr; addr = addr->next) {
				if(addr->addr.type == ADDR_TYPE_IP) {
					if(*(ipv4_addr *)&addr->addr.addr[0] == ntohl(arp->target_ipv4)) {
						iaddr = addr;
					}
				}
			}

			if(!iaddr)
				break; // drop it

			if(iaddr) {
				// the arp was for us, lets respond
				memcpy(&arp->target_ethernet, &arp->sender_ethernet, 6);
				arp->target_ipv4 = arp->sender_ipv4;
				memcpy(&arp->sender_ethernet, &i->link_addr->addr.addr[0], 6);
				arp->sender_ipv4 = htonl(*(ipv4_addr *)&iaddr->addr.addr[0]);
				arp->op = htons(ARP_OP_REPLY);

				dprintf("arp_receive: arp was for us, responding...\n");

				// send it out
				ethernet_output(buf, i, arp->target_ethernet, PROT_TYPE_ARP);

				// the chain gets deleted in the output, so we can return here
				return 0;
			}
			break;
		}
		default:
			dprintf("arp_receive: unhandled arp request type 0x%x\n", ntohs(arp->op));
	}

out:
	cbuf_free_chain(buf);

	return 0;
}

static int arp_send_request(ifnet *i, ipv4_addr sender_ipaddr, ipv4_addr ip_addr)
{
	cbuf *buf;
	arp_packet *arp;

	buf = cbuf_get_chain(sizeof(arp_packet));
	if(!buf)
		return ERR_NO_MEMORY;

	arp = (arp_packet *)cbuf_get_ptr(buf, 0);

	arp->hard_type = htons(ARP_HARD_TYPE_ETHERNET);
	arp->prot_type = htons(PROT_TYPE_IPV4);
	arp->hard_size = 6;
	arp->prot_size = 4;
	arp->op = ntohs(ARP_OP_REQUEST);
	memcpy(&arp->sender_ethernet, &i->link_addr->addr.addr[0], 6);
	arp->sender_ipv4 = htonl(sender_ipaddr);
	memset(&arp->target_ethernet, 0, 6);
	arp->target_ipv4 = htonl(ip_addr);

	return ethernet_broadcast_output(buf, i, PROT_TYPE_ARP);
}

int arp_lookup(ifnet *i, ipv4_addr sender_ipaddr, ipv4_addr ip_addr, ethernet_addr e_addr)
{
	arp_cache_entry *e;
	arp_wait_request wait;
	int err;

	// look in the arp table first
	mutex_lock(&arp_table_mutex);
	e = hash_lookup(arp_table, &ip_addr);
	if(e) {
		memcpy(e_addr, &e->link_addr.addr[0], 6);
		e->last_used_time = system_time();
	}
	mutex_unlock(&arp_table_mutex);

	if(e)
		return 0;

	// guess we're gonna have to send an arp request and wait

	// put ourself in the wait list
	wait.blocking_sem = sem_create(0, "arp_wait");
	wait.attempt_count = 0;
	wait.ip_addr = ip_addr;
	mutex_lock(&arp_wait_mutex);
	if(arp_waiters)
		arp_waiters->prev = &wait;
	arp_waiters = &wait;
	wait.next = arp_waiters;
	wait.prev = NULL;
	mutex_unlock(&arp_wait_mutex);

retry_arp:
	for(; wait.attempt_count < 5; wait.attempt_count++) {
		// send the arp request
		arp_send_request(i, sender_ipaddr, ip_addr);

		// wait, and see if we timeout, or someone released us
		err = sem_acquire_etc(wait.blocking_sem, 1, SEM_FLAG_TIMEOUT, 1000000, NULL);
		if(err >= 0)
			break;
	}
	if(wait.attempt_count >= 5) {
		err = ERR_NET_FAILED_ARP;
		goto out;
	}

	// look it up in the table again
	mutex_lock(&arp_table_mutex);
	e = hash_lookup(arp_table, &ip_addr);
	if(e) {
		memcpy(e_addr, &e->link_addr.addr[0], 6);
		e->last_used_time = system_time();
	}
	mutex_unlock(&arp_table_mutex);

	// see if we found it
	if(e)
		err = 0;
	else
		goto retry_arp;

out:
	// remove ourself from the wait list
	mutex_lock(&arp_wait_mutex);
	if(wait.prev)
		wait.prev->next = wait.next;
	else
		arp_waiters = wait.next;
	if(wait.next)
		wait.next->prev = wait.prev;
	mutex_unlock(&arp_wait_mutex);

	// deallocate the semaphore
	sem_delete(wait.blocking_sem);

	return err;
}

int arp_insert(ipv4_addr ip_addr, netaddr *link_addr)
{
	arp_cache_entry *e;
	arp_wait_request *wait;

#if 1
	dprintf("arp_insert: ip addr ");
	dump_ipv4_addr(ip_addr);
	dprintf(" link_addr: type %d len %d addr ", link_addr->type, link_addr->len);
	dump_ethernet_addr(*(ethernet_addr *)&link_addr->addr[0]);
	dprintf("\n");
#endif

	// see if it's already there
	mutex_lock(&arp_table_mutex);
	e = hash_lookup(arp_table, &ip_addr);
	if(e) {
		if(e->link_addr.type == link_addr->type) {
			if(cmp_netaddr(&e->link_addr, link_addr)) {
				// replace this entry
				hash_remove(arp_table, e);
				mutex_unlock(&arp_table_mutex);
				goto use_old_entry;
			}
		}
	}
	mutex_unlock(&arp_table_mutex);

	// see if it's already there
	if(e)
		return 0;

	e = kmalloc(sizeof(arp_cache_entry));
	if(!e)
		return ERR_NO_MEMORY;

use_old_entry:
	// create the arp entry
	e->ip_addr = ip_addr;
	memcpy(&e->link_addr, link_addr, sizeof(netaddr));
	e->last_used_time = system_time();

	// insert it into the arp cache
	mutex_lock(&arp_table_mutex);
	hash_insert(arp_table, e);
	mutex_unlock(&arp_table_mutex);

	// now, cycle through the arp wait list and wake up any threads that were waiting on
	// an arp entry showing up in the arp cache
	mutex_lock(&arp_wait_mutex);
	for(wait = arp_waiters; wait; wait = wait->next) {
		if(wait->ip_addr == ip_addr)
			sem_release(wait->blocking_sem, 1);
	}
	mutex_unlock(&arp_wait_mutex);

	return 0;
}

static int arp_cleanup_thread(void *unused)
{
	struct hash_iterator i;
	arp_cache_entry *e;
	time_t now;

	for(;;) {
		thread_snooze(1000000 * 60); // 1 min
		mutex_lock(&arp_table_mutex);
		now = system_time();

		hash_open(arp_table, &i);
		for(e = hash_next(arp_table, &i); e; e = hash_next(arp_table, &i)) {
			if(now - e->last_used_time > 1000000LL * 60 * 5) {
				dprintf("arp_cleanup_thread: pruning arp entry for ");
				dump_ipv4_addr(e->ip_addr);
				dprintf("\n");

				hash_remove(arp_table, e);
				kfree(e);
			}
		}
		mutex_unlock(&arp_table_mutex);
	}
}

int arp_init(void)
{
	arp_cache_entry *e;

	mutex_init(&arp_table_mutex, "arp_table_mutex");
	mutex_init(&arp_wait_mutex, "arp_wait_mutex");

	arp_waiters = NULL;

	arp_table = hash_init(256, (addr)&e->next - (addr)e, &arp_cache_compare, &arp_cache_hash);
	if(!arp_table)
		return -1;

	thread_resume_thread(thread_create_kernel_thread("arp cache cleaner", &arp_cleanup_thread, NULL));

	return 0;
}

