/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/cbuf.h>
#include <kernel/lock.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/khash.h>
#include <kernel/sem.h>
#include <kernel/arch/cpu.h>
#include <kernel/net/tcp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/misc.h>
#include <string.h>

typedef struct tcp_header {
	uint16 source_port;
	uint16 dest_port;
	uint32 seq_num;
	uint32 ack_num;
	uint16 length_flags;
	uint16 win_size;
	uint16 checksum;
	uint16 urg_pointer;
} _PACKED tcp_header;

typedef struct tcp_pseudo_header {
	ipv4_addr source_addr;
	ipv4_addr dest_addr;
	uint8 zero;
	uint8 protocol;
	uint16 tcp_length;
} _PACKED tcp_pseudo_header;

typedef enum tcp_state {
	STATE_CLOSED,
	STATE_LISTEN,
	STATE_SYN_SENT,
	STATE_SYN_RCVD,
	STATE_ESTABLISHED,
	STATE_CLOSE_WAIT,
	STATE_LAST_ACK,
	STATE_CLOSING,
	STATE_FIN_WAIT_1,
	STATE_FIN_WAIT_2,
	STATE_TIME_WAIT
} tcp_state;

typedef enum tcp_flags {
	PKT_FIN = 1,
	PKT_SYN = 2,
	PKT_RST = 4,
	PKT_PSH = 8,
	PKT_ACK = 16,
	PKT_URG = 32
} tcp_flags;

typedef struct tcp_socket {
	struct tcp_socket *next;
	tcp_state state;
	mutex lock;
	int ref_count;

	ipv4_addr local_addr;
	ipv4_addr remote_addr;
	uint16 local_port;
	uint16 remote_port;

} tcp_socket;

typedef struct tcp_socket_key {
	ipv4_addr local_addr;
	ipv4_addr remote_addr;
	uint16 local_port;
	uint16 remote_port;
} tcp_socket_key;

#if 0
typedef struct udp_queue_elem {
	struct udp_queue_elem *next;
	struct udp_queue_elem *prev;
	ipv4_addr src_address;
	ipv4_addr target_address;
	uint16 src_port;
	uint16 target_port;
	int len;
	cbuf *buf;
} udp_queue_elem;

typedef struct udp_queue {
	udp_queue_elem *next;
	udp_queue_elem *prev;
	int count;
} udp_queue;

typedef struct udp_endpoint {
	struct udp_endpoint *next;
	mutex lock;
	sem_id blocking_sem;
	uint16 port;
	udp_queue q;
	int ref_count;
} udp_endpoint;
#endif

static tcp_socket *socket_table;
static mutex socket_table_lock;

// forward decls
static void tcp_send(ipv4_addr dest_addr, uint16 dest_port, ipv4_addr src_addr, uint16 source_port, cbuf *buf, tcp_flags flags,
	uint32 ack, const void *options, uint16 options_length, uint32 sequence, uint16 window_size);
static int destroy_tcp_socket(tcp_socket *s);
static tcp_socket *create_tcp_socket(void);


static int tcp_socket_compare_func(void *_s, const void *_key)
{
	tcp_socket *s = _s;
	const tcp_socket_key *key = _key;

	if(s->local_addr == key->local_addr &&
	   s->remote_addr == key->remote_addr &&
	   s->local_port == key->local_port &&
	   s->remote_port == key->remote_port)
		return 0;
	else
		return 1;
}

static unsigned int tcp_socket_hash_func(void *_s, const void *_key, unsigned int range)
{
	tcp_socket *s = _s;
	const tcp_socket_key *key = _key;
	unsigned int hash;

	if(s) {
		hash = *(uint32 *)&s->local_addr ^ *(uint32 *)&s->remote_addr ^ s->local_port ^ s->remote_port;
	} else {
		hash = *(uint32 *)&key->local_addr ^ *(uint32 *)&key->remote_addr ^ key->local_port ^ key->remote_port;
	}

	return hash % range;
}

static void inc_socket_ref(tcp_socket *s)
{
	if(atomic_add(&s->ref_count, 1) <= 0)
		panic("inc_socket_ref: socket %p has bad ref %d\n", s, s->ref_count);
}

static void dec_socket_ref(tcp_socket *s)
{
	if(atomic_add(&s->ref_count, -1) == 1)
		destroy_tcp_socket(s);
}

static tcp_socket *lookup_socket(ipv4_addr src_addr, ipv4_addr dest_addr, uint16 src_port, uint16 dest_port)
{
	tcp_socket_key key;
	tcp_socket *s;

	key.local_addr = dest_addr;
	key.local_port = dest_port;

	// first search for a socket matching the remote address
	key.remote_addr = src_addr;
	key.remote_port = src_port;

	mutex_lock(&socket_table_lock);

	s = hash_lookup(socket_table, &key);
	if(s)
		goto found;

	// didn't see it, lets search for the null remote address (a socket in accept state)
	key.remote_addr = 0;
	key.remote_port = 0;

	s = hash_lookup(socket_table, &key);
	if(!s)
		goto out;

found:
	inc_socket_ref(s);
out:
	mutex_unlock(&socket_table_lock);
	return s;
}

static tcp_socket *create_tcp_socket(void)
{
	tcp_socket *s;

	s = kmalloc(sizeof(tcp_socket));
	if(!s)
		return NULL;

	// set up the new socket structure
	s->next = NULL;
	s->state = STATE_CLOSED;
	if(mutex_init(&s->lock, "socket lock") < 0) {
		kfree(s);
		return NULL;
	}
	s->ref_count = 1;
	s->local_addr = 0;
	s->local_port = 0;
	s->remote_addr = 0;
	s->remote_port = 0;

	return s;
}

static int destroy_tcp_socket(tcp_socket *s)
{
	mutex_destroy(&s->lock);
	kfree(s);
}

int tcp_input(cbuf *buf, ifnet *i, ipv4_addr source_address, ipv4_addr target_address)
{
	tcp_header *header;
	uint16 port;
	int err = 0;
	int length = cbuf_get_len(buf);
	tcp_socket *s = NULL;
	uint8 packet_flags;

	header = cbuf_get_ptr(buf, 0);

#if NET_CHATTY
	dprintf("tcp_input: src port %d, dest port %d, buf len %d, checksum 0x%x\n",
		ntohs(header->source_port), ntohs(header->dest_port), (int)cbuf_get_len(buf), ntohs(header->checksum));
#endif

	// deal with the checksum check
	if(header->checksum) {
		tcp_pseudo_header pheader;
		uint16 checksum;

		// set up the pseudo header for checksum purposes
		pheader.source_addr = htonl(source_address);
		pheader.dest_addr = htonl(target_address);
		pheader.zero = 0;
		pheader.protocol = IP_PROT_TCP;
		pheader.tcp_length = htons(length);

		if(length % 2) {
			// make sure the pad byte is zero
			((uint8 *)header)[length] = 0;
		}
		checksum = cksum16_2(&pheader, sizeof(pheader), header, ROUNDUP(length, 2));
		if(checksum != 0) {
#if NET_CHATTY
			dprintf("tcp_receive: packet failed checksum\n");
#endif
			err = ERR_NET_BAD_PACKET;
			goto ditch_packet;
		}
	}

	// get some data from the packet
	packet_flags = ntohs(header->length_flags) & 0x3f;

	// see if it matches a socket we have
	s = lookup_socket(source_address, target_address, ntohs(header->source_port), ntohs(header->dest_port));
	if(!s) {
		// send a RST packet
		goto send_reset;
	}

	// lock the socket
	mutex_lock(&s->lock);

	switch(s->state) {
		case STATE_CLOSED:
			// socket is closed, send RST packet
			goto send_reset;
		case STATE_LISTEN: {
			tcp_socket *accept_socket;

			if(!(packet_flags & PKT_SYN)) {
				// didn't have a SYN flag, send a reset
				goto send_reset;
			}

			// XXX finish me
			goto send_reset;

			break;
		}
		case STATE_SYN_SENT:
		case STATE_SYN_RCVD:
		case STATE_ESTABLISHED:
		case STATE_CLOSE_WAIT:
		case STATE_LAST_ACK:
		case STATE_CLOSING:
		case STATE_FIN_WAIT_1:
		case STATE_FIN_WAIT_2:
		case STATE_TIME_WAIT:
		default:
			dprintf("tcp_receive: incoming packet on socket with unhandled state %d\n", s->state);
			goto ditch_packet;
	}

send_reset:
	if(!(packet_flags & PKT_RST))
		tcp_send(source_address, ntohs(header->source_port), target_address, ntohs(header->dest_port),
			NULL, PKT_RST|PKT_ACK, ntohl(header->seq_num) + 1, NULL, 0, ntohl(header->ack_num), 0);
ditch_packet:
	cbuf_free_chain(buf);
	if(s) {
		mutex_unlock(&s->lock);
		dec_socket_ref(s);
	}


	return err;
}

int tcp_open(netaddr *addr, uint16 port, void **prot_data)
{
	tcp_socket *s;

	s = create_tcp_socket();
	if(!s)
		return ERR_NO_MEMORY;



	return 0;
}

int tcp_close(void *prot_data)
{

	return 0;
}


#if 0
int udp_open(netaddr *addr, uint16 port, void **prot_data)
{
	udp_endpoint *e;

	e = kmalloc(sizeof(udp_endpoint));
	if(!e)
		return ERR_NO_MEMORY;

	mutex_init(&e->lock, "udp endpoint lock");
	e->blocking_sem = sem_create(0, "udp endpoint sem");
	e->port = port;
	e->ref_count = 1;
	udp_init_queue(&e->q);

	mutex_lock(&endpoints_lock);
	hash_insert(endpoints, e);
	mutex_unlock(&endpoints_lock);

	*prot_data = e;

	return 0;
}

int udp_close(void *prot_data)
{
	udp_endpoint *e = prot_data;

	mutex_lock(&endpoints_lock);
	hash_remove(endpoints, e);
 	mutex_unlock(&endpoints_lock);

	udp_endpoint_release_ref(e);

	return 0;
}

ssize_t udp_recvfrom(void *prot_data, void *buf, ssize_t len, sockaddr *saddr, int flags, bigtime_t timeout)
{
	udp_endpoint *e = prot_data;
	udp_queue_elem *qe;
	int err;
	ssize_t ret;

retry:
	if(flags & SOCK_FLAG_TIMEOUT)
		err = sem_acquire_etc(e->blocking_sem, 1, SEM_FLAG_TIMEOUT, timeout, NULL);
	else
		err = sem_acquire(e->blocking_sem, 1);
	if(err < 0)
		return err;

	// pop an item off the list, if there are any
	mutex_lock(&e->lock);
	qe = udp_queue_pop(&e->q);
	mutex_unlock(&e->lock);

	if(!qe)
		goto retry;

	// we have the data, copy it out
	cbuf_memcpy_from_chain(buf, qe->buf, 0, min(qe->len, len));
	ret = qe->len;

	// copy the address out
	if(saddr) {
		saddr->addr.len = 4;
		saddr->addr.type = ADDR_TYPE_IP;
		NETADDR_TO_IPV4(&saddr->addr) = qe->src_address;
		saddr->port = qe->src_port;
	}

	// free this queue entry
	cbuf_free_chain(qe->buf);
	kfree(qe);

	return ret;
}

ssize_t udp_sendto(void *prot_data, const void *inbuf, ssize_t len, sockaddr *toaddr)
{
	udp_endpoint *e = prot_data;
	udp_header *header;
	int total_len;
	cbuf *buf;
	uint8 *bufptr;
	udp_pseudo_header pheader;
	ipv4_addr srcaddr;

	// make sure the args make sense
	if(len < 0 || len + sizeof(udp_header) > 0xffff)
		return ERR_INVALID_ARGS;
	if(toaddr->port < 0 || toaddr->port > 0xffff)
		return ERR_INVALID_ARGS;

	// allocate a buffer to hold the data + header
	total_len = len + sizeof(udp_header);
	buf = cbuf_get_chain(total_len);
	if(!buf)
		return ERR_NO_MEMORY;

	// copy the data to this new buffer
	cbuf_memcpy_to_chain(buf, sizeof(udp_header), inbuf, len);

	// set up the udp pseudo header
	if(ipv4_lookup_srcaddr_for_dest(NETADDR_TO_IPV4(&toaddr->addr), &srcaddr) < 0) {
		cbuf_free_chain(buf);
		return ERR_NET_NO_ROUTE;
	}
	pheader.source_addr = htonl(srcaddr);
	pheader.dest_addr = htonl(NETADDR_TO_IPV4(&toaddr->addr));
	pheader.zero = 0;
	pheader.protocol = IP_PROT_UDP;
	pheader.udp_length = htons(total_len);

	// start setting up the header
	header = cbuf_get_ptr(buf, 0);
	header->source_port = htons(e->port);
	header->dest_port = htons(toaddr->port);
	header->length = htons(total_len);
	header->checksum = 0;
	header->checksum = cksum16_2(&pheader, sizeof(pheader), header, total_len);
	if(header->checksum == 0)
		header->checksum = 0xffff;

	// send it away
	return ipv4_output(buf, NETADDR_TO_IPV4(&toaddr->addr), IP_PROT_UDP);
}
#endif

static void tcp_send(ipv4_addr dest_addr, uint16 dest_port, ipv4_addr src_addr, uint16 source_port, cbuf *buf, tcp_flags flags,
	uint32 ack, const void *options, uint16 options_length, uint32 sequence, uint16 window_size)
{
	tcp_pseudo_header pheader;
	tcp_header *header;
	cbuf *header_buf;

	// grab a buf large enough to hold the header + options
	header_buf = cbuf_get_chain(sizeof(tcp_header) + options_length);
	if(!header_buf)
		goto error;

	header = (tcp_header *)cbuf_get_ptr(header_buf, 0);
	header->ack_num = htonl(ack);
	header->dest_port = htons(dest_port);
	header->length_flags = htons(((sizeof(tcp_header) + options_length) / 4) << 12 | flags);
	header->seq_num = htonl(sequence);
	header->source_port = htons(source_port);
	header->urg_pointer = 0;
	if(options)
		memcpy(header + 1, options, options_length);
	header->win_size = htons(window_size);

	header_buf = cbuf_merge_chains(header_buf, buf);

	// checksum
	pheader.source_addr = htonl(src_addr);
	pheader.dest_addr = htonl(dest_addr);
	pheader.zero = 0;
	pheader.protocol = IP_PROT_TCP;
	pheader.tcp_length = htons(cbuf_get_len(header_buf));

	header->checksum = 0;
	header->checksum = cbuf_ones_cksum16_2(header_buf, &pheader, sizeof(pheader), 0, cbuf_get_len(header_buf));

	ipv4_output(header_buf, dest_addr, IP_PROT_TCP);
	return;

error:
	cbuf_free_chain(buf);
}


int tcp_init(void)
{
	tcp_socket s;

	mutex_init(&socket_table_lock, "tcp socket table lock");

	socket_table = hash_init(256, (addr)&s.next - (addr)&s, &tcp_socket_compare_func, &tcp_socket_hash_func);
	if(!socket_table)
		return ERR_NO_MEMORY;
	return 0;
}

