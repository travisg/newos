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
#include <kernel/net/net_timer.h>
#include <string.h>
#include <stdlib.h>

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

typedef struct tcp_mss_option {
	uint8 kind; /* 0x2 */
	uint8 len;  /* 0x4 */
	uint16 mss;
} _PACKED tcp_mss_option;

typedef struct tcp_window_scale_option {
	uint8 kind; /* 0x3 */
	uint8 len;  /* 0x3 */
	uint8 shift_count;
} _PACKED tcp_window_scale_option;

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

	uint32 mss;

	/* rx */
	sem_id read_sem;
	uint32 rx_win_size;
	uint32 rx_win_low;
	uint32 rx_win_high;
	cbuf *reassembly_q;
	cbuf *read_buffer;
	net_timer_event ack_delay_timer;

	/* tx */
	sem_id write_sem;
	uint32 tx_win_low;

} tcp_socket;

typedef struct tcp_socket_key {
	ipv4_addr local_addr;
	ipv4_addr remote_addr;
	uint16 local_port;
	uint16 remote_port;
} tcp_socket_key;

static tcp_socket *socket_table;
static mutex socket_table_lock;
static int next_ephemeral_port = 1024;

#define SYN_RETRANSMIT_TIMEOUT 1000000
#define ACK_DELAY 500
#define DEFAULT_RX_WINDOW_SIZE (32*1024)
#define DEFAULT_MAX_SEGMENT_SIZE 1024

#define SEQUENCE_GTE(a, b) ((int)((a) - (b)) >= 0)
#define SEQUENCE_LTE(a, b) ((int)((a) - (b)) <= 0)
#define SEQUENCE_GT(a, b) ((int)((a) - (b)) > 0)
#define SEQUENCE_LT(a, b) ((int)((a) - (b)) < 0)

// forward decls
static void tcp_send(ipv4_addr dest_addr, uint16 dest_port, ipv4_addr src_addr, uint16 source_port, cbuf *buf, tcp_flags flags,
	uint32 ack, const void *options, uint16 options_length, uint32 sequence, uint16 window_size);
static void tcp_socket_send(tcp_socket *s, cbuf *data, tcp_flags flags, const void *options, uint16 options_length, uint32 sequence);
static void handle_ack(tcp_socket *s, uint32 sequence, uint32 window_size, bool with_data);
static void handle_data(tcp_socket *s, cbuf *buf);
static void handle_ack_delay_timeout(void *_socket);
static int destroy_tcp_socket(tcp_socket *s);
static tcp_socket *create_tcp_socket(void);
static void send_ack(tcp_socket *s);
static void tcp_remote_close(tcp_socket *s);


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

	memset(s, 0, sizeof(tcp_socket));

	// set up the new socket structure
	s->next = NULL;
	s->state = STATE_CLOSED;
	if(mutex_init(&s->lock, "socket lock") < 0)
		goto err;
	s->read_sem = sem_create(0, "socket read sem");
	if(s->read_sem < 0)
		goto err1;
	s->write_sem = sem_create(0, "socket write sem");
	if(s->write_sem < 0)
		goto err2;
	s->ref_count = 1;
	s->local_addr = 0;
	s->local_port = 0;
	s->remote_addr = 0;
	s->remote_port = 0;
	s->mss = DEFAULT_MAX_SEGMENT_SIZE;
	s->rx_win_size = DEFAULT_RX_WINDOW_SIZE;
	s->rx_win_low = 0;
	s->rx_win_high = 0;
	s->tx_win_low = rand();

	return s;

err2:
	sem_delete(s->read_sem);
err1:
	mutex_destroy(&s->lock);
err:
	kfree(s);
	return NULL;
}

static int destroy_tcp_socket(tcp_socket *s)
{
	sem_delete(s->read_sem);
	mutex_destroy(&s->lock);
	kfree(s);
}

static int bind_local_address(tcp_socket *s, netaddr *remote_addr)
{
	int err = 0;

	// find the local ip address to bind this socket to
	if(s->local_addr == 0) {
		err = ipv4_lookup_srcaddr_for_dest(NETADDR_TO_IPV4(*remote_addr), &s->local_addr);
		if(err < 0)
			return err;
	}

	// find a local port to bind this socket to
	// XXX hack hack hack
	if(s->local_port == 0) {
		s->local_port = atomic_add(&next_ephemeral_port, 1);
		if(s->local_port >= 0x10000)
			s->local_port = 0;
	}

	return err;
}

int tcp_input(cbuf *buf, ifnet *i, ipv4_addr source_address, ipv4_addr target_address)
{
	tcp_header *header;
	uint16 port;
	int err = 0;
	int length = cbuf_get_len(buf);
	tcp_socket *s = NULL;
	uint8 packet_flags;
	uint16 header_len;
	uint16 data_len;

	header = cbuf_get_ptr(buf, 0);

#if NET_CHATTY
	dprintf("tcp_input: src port %d, dest port %d, buf len %d, checksum 0x%x, flags 0x%x\n",
		ntohs(header->source_port), ntohs(header->dest_port), (int)cbuf_get_len(buf), ntohs(header->checksum), ntohs(header->length_flags) & 0x3f);
#endif

	// deal with the checksum check
	{
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
	header_len = ((ntohs(header->length_flags) >> 12) & 0x0f) * 4;
	data_len = cbuf_get_len(buf) - header_len;

	// see if it matches a socket we have
	s = lookup_socket(source_address, target_address, ntohs(header->source_port), ntohs(header->dest_port));
	if(!s) {
		// send a RST packet
		goto send_reset;
	}

	// lock the socket
	mutex_lock(&s->lock);

	// see if the other side wants to reset the connection
	if(packet_flags & PKT_RST) {
		if(s->state != STATE_CLOSED && s->state != STATE_LISTEN) {
			tcp_remote_close(s);
		}
		goto ditch_packet;
	}

	// check for out of window packets
	if(!(packet_flags & PKT_SYN)) {
		if(SEQUENCE_LT(ntohl(header->seq_num), s->rx_win_low)
			|| SEQUENCE_GT(ntohl(header->seq_num), s->rx_win_high)) {
			/* out of window, ack it */
			send_ack(s);
			goto ditch_packet;
		}
	}

#if NET_CHATTY
	dprintf("tcp_input: socket %p, state 0x%x\n", s, s->state);
#endif

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
			s->tx_win_low++;
			if(packet_flags & PKT_SYN) {
				s->rx_win_low = ntohl(header->seq_num) + 1;
				s->rx_win_high = s->rx_win_low + s->rx_win_size - 1;
				if(packet_flags & PKT_ACK) {
					// they're acking our SYN
					if(ntohl(header->ack_num) != s->tx_win_low)
						goto send_reset;

					tcp_socket_send(s, NULL, PKT_ACK, NULL, 0, s->tx_win_low);
					s->state = STATE_ESTABLISHED;
					sem_release(s->read_sem, 1);
				} else {
					// simultaneous open
					// XXX handle
					goto send_reset;
				}
			} else {
				s->state = STATE_CLOSED;
				goto ditch_packet;
			}
			break;
		case STATE_ESTABLISHED: {
			if(packet_flags & PKT_ACK)
				handle_ack(s, ntohl(header->seq_num), ntohs(header->win_size), data_len > 0);

			if(data_len > 0) {
				handle_data(s, buf);
				buf = NULL; // handle_data will deal with the buffer from now on
			}

			if(packet_flags & PKT_FIN) {
				// XXX handle this
			}
			break;
		}
		case STATE_SYN_RCVD:
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

	err = NO_ERROR;
	goto ditch_packet;

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

int tcp_open(void **prot_data)
{
	tcp_socket *s;

	s = create_tcp_socket();
	if(!s)
		return ERR_NO_MEMORY;

	*prot_data = s;

	return NO_ERROR;
}

int tcp_bind(void *prot_data, sockaddr *addr)
{
	tcp_socket *s = prot_data;
	int err = 0;

	inc_socket_ref(s);
	mutex_lock(&s->lock);

	if(s->local_port != 0 || s->local_addr != 0) {
		err = ERR_NET_SOCKET_ALREADY_BOUND;
		goto out;
	}

	mutex_lock(&socket_table_lock);
	hash_remove(socket_table, s);

	// XXX check to see if this address is used or makes sense
	s->local_port = addr->port;
	s->local_addr = NETADDR_TO_IPV4(addr->addr);

	hash_insert(socket_table, s);
	mutex_unlock(&socket_table_lock);

out:
	mutex_unlock(&s->lock);
	dec_socket_ref(s);

	return 0;
}

int tcp_connect(void *prot_data, sockaddr *addr)
{
	tcp_socket *s = prot_data;
	int err;
	int i;
	tcp_mss_option mss_option;

	inc_socket_ref(s);
	mutex_lock(&s->lock);

	// see if the socket can be connected
	if(s->state > STATE_CLOSED) {
		err = ERR_NET_ALREADY_CONNECTED;
		goto out;
	}

	// pull the socket out of the hash table
	mutex_lock(&socket_table_lock);
	hash_remove(socket_table, s);
	mutex_unlock(&socket_table_lock);

	// allocate a local address, if needed
	if(s->local_port == 0 || s->local_addr == 0) {
		err = bind_local_address(s, &addr->addr);
		if(err < 0)
			goto out;
	}

	s->remote_addr = NETADDR_TO_IPV4(addr->addr);
	s->remote_port = addr->port;

	mutex_lock(&socket_table_lock);
	hash_insert(socket_table, s);
	mutex_unlock(&socket_table_lock);

	// figure out what the mss will be
	err = ipv4_get_mss_for_dest(s->remote_addr, &s->mss);
	if(err < 0)
		s->mss = DEFAULT_MAX_SEGMENT_SIZE;

	// set up the mss option
	mss_option.kind = 0x2;
	mss_option.len = 0x4;
	mss_option.mss = htons(s->mss);

	// welcome to the machine
	s->state = STATE_SYN_SENT;
	for(i=0; i < 3 && s->state != STATE_ESTABLISHED && s->state != STATE_CLOSED; i++) {
		if(s->state == STATE_SYN_SENT)
			tcp_socket_send(s, NULL, PKT_SYN, &mss_option, sizeof(mss_option), s->tx_win_low);
		mutex_unlock(&s->lock);
		sem_acquire_etc(s->read_sem, 1, SEM_FLAG_TIMEOUT, SYN_RETRANSMIT_TIMEOUT, NULL);
		mutex_lock(&s->lock);
	}

	if(s->state == STATE_CLOSED) {
		err = ERR_NET_CONNECTION_REFUSED;
		goto out;
	}

	err = NO_ERROR;

out:
	mutex_unlock(&s->lock);
	dec_socket_ref(s);

	return err;
}

int tcp_close(void *prot_data)
{

	return 0;
}

ssize_t tcp_recvfrom(void *prot_data, void *buf, ssize_t len, sockaddr *saddr, int flags, bigtime_t timeout)
{
	tcp_socket *s = prot_data;
	int err;
	ssize_t bytes_read = 0;
	int avail;
	ssize_t to_copy;
	uint32 new_rx_win_size;

	inc_socket_ref(s);
	mutex_lock(&s->lock);

	if(s->state != STATE_ESTABLISHED) {
		bytes_read = 0;
		goto out;
	}

	/* wait for the buffer to have something in it, or timeout */
	while(s->state == STATE_ESTABLISHED && s->read_buffer == NULL) {
		mutex_unlock(&s->lock);
		if(flags & SOCK_FLAG_TIMEOUT)
			err = sem_acquire_etc(s->read_sem, 1, SEM_FLAG_TIMEOUT, timeout, NULL);
		else
			err = sem_acquire(s->read_sem, 1);
		mutex_lock(&s->lock);
		if(err < 0) {
			bytes_read = err;
			goto out;
		}
	}

	/* copy as much data as we can */
	avail = cbuf_get_len(s->read_buffer);
	to_copy = min(avail, len);
	err = cbuf_user_memcpy_from_chain(buf, s->read_buffer, 0, to_copy);
	if(err < 0) {
		bytes_read = err;
		goto out;
	}

	/* truncate the read buffer */
	s->read_buffer = cbuf_truncate_head(s->read_buffer, to_copy);
	if(cbuf_get_len(s->read_buffer) == 0) {
		cbuf_free_chain(s->read_buffer);
		s->read_buffer = NULL;
	}
	bytes_read = to_copy;

	/* update the receive window */
	new_rx_win_size = s->rx_win_size - cbuf_get_len(s->read_buffer);

	/* see if the window is opening, and needs an ack to be sent */
	if(new_rx_win_size >= s->mss && s->rx_win_high - s->rx_win_low < s->mss)
		send_ack(s);

out:
	mutex_unlock(&s->lock);
	dec_socket_ref(s);
	return bytes_read;
}

ssize_t tcp_sendto(void *prot_data, const void *inbuf, ssize_t len, sockaddr *toaddr)
{

	return ERR_UNIMPLEMENTED;
}

#if 0
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

static void send_ack(tcp_socket *s)
{
	ASSERT_LOCKED_MUTEX(&s->lock);

	if(cancel_net_timer(&s->ack_delay_timer) >= 0)
		dec_socket_ref(s);

	if(s->state != STATE_ESTABLISHED)
		return;

	// XXX handle sending any pending data here, have the ack piggyback that
	tcp_socket_send(s, NULL, PKT_ACK, NULL, 0, s->tx_win_low);
}


static void handle_ack(tcp_socket *s, uint32 sequence, uint32 window_size, bool with_data)
{
	// XXX implement
	ASSERT_LOCKED_MUTEX(&s->lock);
}

static void handle_ack_delay_timeout(void *_socket)
{
	tcp_socket *s = (tcp_socket *)_socket;

	mutex_lock(&s->lock);
	send_ack(s);
	mutex_unlock(&s->lock);
	dec_socket_ref(s);
}

static void handle_data(tcp_socket *s, cbuf *buf)
{
	tcp_header header;
	int header_length;
	uint32 seq_low, seq_high;

	ASSERT_LOCKED_MUTEX(&s->lock);

	// copy the header
	memcpy(&header, cbuf_get_ptr(buf, 0), sizeof(header));
	header_length = ((ntohs(header.length_flags) >> 12) % 0xf) * 4;
	seq_low = ntohl(header.seq_num);
	seq_high = seq_low + cbuf_get_len(buf) - header_length - 1;

	if(SEQUENCE_LTE(seq_low, s->rx_win_low) && SEQUENCE_GTE(seq_high, s->rx_win_low)) {
		// it's in order, so truncate from the head and add to the receive buffer
		buf = cbuf_truncate_head(buf, header_length + (s->rx_win_low - seq_low));
		s->rx_win_low += cbuf_get_len(buf);
		s->read_buffer = cbuf_merge_chains(s->read_buffer, buf);

		sem_release(s->read_sem, 1);

		// see if any reassembly packets can now be dealt with
		while(s->reassembly_q) {
			tcp_header *q_header = (tcp_header *)cbuf_get_ptr(s->reassembly_q, 0);
			int packet_header_len = ((ntohs(q_header->length_flags) >> 12) % 0xf) * 4;
			uint32 packet_low = ntohl(q_header->seq_num);
			uint32 packet_high = packet_low + cbuf_get_len(s->reassembly_q) - packet_header_len;

			if(SEQUENCE_LT(packet_high, s->rx_win_low)) {
				/* this packet is totally out of window */
				cbuf *tmp = s->reassembly_q;
				s->reassembly_q = tmp->packet_next;
				cbuf_free_chain(tmp);
			} else if(SEQUENCE_LTE(packet_low, s->rx_win_low)) {
				/* a portion of this packet may be useful now */
				cbuf *tmp = s->reassembly_q;
				s->reassembly_q = tmp->packet_next;

				tmp = cbuf_truncate_head(tmp, packet_header_len + (s->rx_win_low - packet_low));
				s->rx_win_low += cbuf_get_len(tmp);

				/* merge it with the read data */
				if(s->read_buffer)
					s->read_buffer = cbuf_merge_chains(s->read_buffer, tmp);
				else
					s->read_buffer = tmp;
			} else {
				break;
			}
		}

		// set up a delayed ack
		if((int)(s->rx_win_low + s->rx_win_size - s->rx_win_high) > (int)s->rx_win_size / 2) {
			send_ack(s);
		} else if(set_net_timer(&s->ack_delay_timer, ACK_DELAY, handle_ack_delay_timeout, s, NET_TIMER_PENDING_IGNORE) >= 0) {
			// a delayed ack timeout was set
			inc_socket_ref(s);
		}
	} else {
		// packet is out of order, stick it on the reassembly queue
		if(s->reassembly_q == NULL ||
		   SEQUENCE_GT(ntohl(((tcp_header *)cbuf_get_ptr(s->reassembly_q, 0))->seq_num), seq_low)) {
			// stick it on the head of the queue
			buf = cbuf_truncate_head(buf, header_length);

			buf->packet_next = NULL;
			s->reassembly_q = buf;
		} else {
			// find the spot in the queue where we need to stick it
			cbuf *last = s->reassembly_q;

			for(; last; last = last->packet_next) {
				cbuf *next = last->packet_next;
				if(next == NULL || SEQUENCE_GT(ntohl(((tcp_header *)cbuf_get_ptr(next, 0))->seq_num), seq_low)) {
					// we found a spot
					buf->packet_next = next;
					last->packet_next = buf;
					break;
				}
			}
		}

		send_ack(s);
	}
}

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

static void tcp_socket_send(tcp_socket *s, cbuf *data, tcp_flags flags, const void *options, uint16 options_length, uint32 sequence)
{
	uint32 rx_win_high;
	uint16 win_size;

	ASSERT_LOCKED_MUTEX(&s->lock);

	// calculate the new right edge of the rx window
	rx_win_high = s->rx_win_low + s->rx_win_size - cbuf_get_len(s->read_buffer) - 1;

#if NET_CHATTY
	dprintf("** s->rx_win_low %ud s->rx_win_size %ud read_buf_len %d, new win high %ud\n",
		s->rx_win_low, s->rx_win_size, cbuf_get_len(s->read_buffer), rx_win_high);
#endif
	if(SEQUENCE_GTE(rx_win_high, s->rx_win_high)) {
		s->rx_win_high = rx_win_high;
		win_size = rx_win_high - s->rx_win_low;
	} else {
		// the window size has shrunk, but we can't move the
		// right edge of the window backwards
		win_size = s->rx_win_high - s->rx_win_low;
	}

	// we are piggybacking a pending ACK, so clear the delayed ACK timer
	if(flags & PKT_ACK) {
		if(cancel_net_timer(&s->ack_delay_timer) == 0)
			dec_socket_ref(s);
	}

	mutex_unlock(&s->lock);
	tcp_send(s->remote_addr, s->remote_port, s->local_addr, s->local_port, data, flags, s->rx_win_low,
			options, options_length, sequence, win_size);
	mutex_lock(&s->lock);
}

static void tcp_remote_close(tcp_socket *s)
{

	ASSERT_LOCKED_MUTEX(&s->lock);

	if(s->state == STATE_CLOSED)
		return;

	inc_socket_ref(s);
	mutex_unlock(&s->lock);

	// pull the socket out of the hash table
	mutex_lock(&socket_table_lock);
	hash_remove(socket_table, s);
	mutex_unlock(&socket_table_lock);

	mutex_lock(&s->lock);

	s->state = STATE_CLOSED;

	dec_socket_ref(s);
}

int tcp_init(void)
{
	tcp_socket s;

	mutex_init(&socket_table_lock, "tcp socket table lock");

	socket_table = hash_init(256, (addr)&s.next - (addr)&s, &tcp_socket_compare_func, &tcp_socket_hash_func);
	if(!socket_table)
		return ERR_NO_MEMORY;

	next_ephemeral_port = rand() % 32000 + 1024;
	return 0;
}

