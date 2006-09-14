/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/lock.h>
#include <kernel/net/socket.h>
#include <kernel/net/misc.h>

#include <string.h>
#include <stdlib.h>

#include "rpc.h"

int rpc_init_state(rpc_state *state)
{
	memset(state, sizeof(rpc_state), 0);

	// create a lock
	mutex_init(&state->lock, "rpc_state");

	state->socket = -1;
	state->auth_cookie = rand();

	return 0;
}

int rpc_destroy_state(rpc_state *state)
{
	socket_close(state->socket);

	mutex_destroy(&state->lock);

	return 0;
}

int rpc_set_port(rpc_state *state, int port)
{
	mutex_lock(&state->lock);

	state->server_addr.port = port;

	mutex_unlock(&state->lock);
}

int rpc_open_socket(rpc_state *state, netaddr *server_addr)
{
	mutex_lock(&state->lock);

	state->socket = socket_create(SOCK_PROTO_UDP, 0);
	if(state->socket < 0) {
		mutex_unlock(&state->lock);
		return state->socket;
	}

	state->server_addr.port = RPC_PORT;
	memcpy(&state->server_addr.addr, server_addr, sizeof(netaddr));

	mutex_unlock(&state->lock);

	return NO_ERROR;
}

int rpc_call(rpc_state *state, unsigned int prog, unsigned int vers, unsigned int proc,
	const void *out_data, int out_data_len, void *in_data, int in_data_len)
{
	struct msg_header *header;
	struct call_body *body;
	struct auth *auth;
	unsigned int xid;
	int len;
	int pos;
	int err = 0;
	sockaddr fromaddr;

	mutex_lock(&state->lock);

	// build the header
	header = (struct msg_header *)&state->buf[0];
	xid = rand();
	header->xid = htonl(xid);
	header->msg_type = htonl(RPC_CALL);

	// this is a call
	body = (struct call_body *)&state->buf[8];

	body->rpcvers = htonl(RPC_VERS);
	body->prog = htonl(prog);
	body->vers = htonl(vers);
	body->proc = htonl(proc);

	len = 24;

	// cred auth
	auth = (struct auth *)&state->buf[len];
	{
		/* XXX do unix auth for now, make this smarter */
		/* unix auth structure (from rfc 1057)
		**	struct auth_unix {
		**		unsigned int stamp;
		**		string machinename<255>;
		**		unsigned int uid;
		**		unsigned int gid;
		**		unsigned int gids<16>;
		**	};
		*/
		int i = 0;

		auth->auth_flavor = htonl(RPC_AUTH_UNIX);
		auth->auth_len = -1; // fill this in later

		// unsigned int stamp;
		auth->data[i++] = state->auth_cookie;
		// string machinename<255>;
		auth->data[i++] = htonl(sizeof("newos")); // len
		memcpy(&auth->data[i], "newos", sizeof("newos"));
		i += ROUNDUP(sizeof("newos"), sizeof(unsigned int)) / sizeof(unsigned int);
		// unsigned int uid;
		auth->data[i++] = htonl(0);
		// unsigned int gid;
		auth->data[i++] = htonl(0);
		// unsigned int gids<16>;
		auth->data[i++] = htonl(0); // len
		auth->auth_len = htonl(i * 4);

		len += i * 4 + 8;
	}

	// verf auth
	auth = (struct auth *)&state->buf[len];
	auth[0].auth_flavor = htonl(RPC_AUTH_NULL);
	auth[0].auth_len = 0;
	len += 8;

	// copy the passed in data to the buffer
	if(out_data_len > 0) {
		memcpy(&state->buf[len], out_data, out_data_len);
		len += out_data_len;
	}

	// send the message
	socket_sendto(state->socket, state->buf, len, &state->server_addr);

	// wait for response
	len = socket_recvfrom_etc(state->socket, state->buf, sizeof(state->buf), &fromaddr, SOCK_FLAG_TIMEOUT, 5000000);
	if(len < 0) {
		err = len;
		dprintf("rpc_call: returned err %d from recv call\n", err);
		goto err;
	}

//	dprintf("received response of len %d\n", len);

	pos = 0;

	// see if it's a response to this call
	header = (struct msg_header *)&state->buf[pos];
	if(ntohl(header->xid) != xid) {
		dprintf("rpc_call: received response from different call (xid 0x%x)\n", ntohl(header->xid));
		err = ERR_GENERAL;
		goto err;
	}
//	dprintf("rpc_call: received message:\n");
//	dprintf("\txid: 0x%x\n", ntohl(header->xid));

	if(ntohl(header->msg_type) != RPC_REPLY) {
		dprintf("rpc_call: did not receive reply\n");
		err = ERR_GENERAL;
		goto err;
	}
//	dprintf("\ttype: REPLY\n");

	pos += 8;
	switch(htonl(*(rpc_reply_stat *)&state->buf[pos])) {
		case RPC_MSG_ACCEPTED:
//			dprintf("\treply stat: ACCEPTED\n");
			pos += 4;
			auth = (struct auth *)&state->buf[pos];
//			dprintf("\tverf auth: flavor %d, len %d\n", ntohl(auth->auth_flavor), ntohl(auth->auth_len));
			pos += 8 + ntohl(auth->auth_len);
			if(htonl(*(rpc_accept_stat *)&state->buf[pos]) == RPC_SUCCESS) {
//				dprintf("\taccept stat: SUCCESS\n");
				pos += 4;
				// good call, copy the remainder of the data to the out buffer
				memcpy(in_data, &state->buf[pos], len - pos);
				err = len - pos;
			} else {
//				dprintf("\taccept stat: %d\n", htonl(*(rpc_accept_stat *)&state->buf[pos]));
				err = ERR_GENERAL;
			}
			break;
		case RPC_MSG_DENIED:
//			dprintf("\treply stat: DENIED\n");
			err = ERR_GENERAL;
			break;
	}

err:
	mutex_unlock(&state->lock);

	return err;
}

