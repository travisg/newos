/*
** Copyright 2002-2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _RPC_H
#define _RPC_H

#include <kernel/kernel.h>
#include <kernel/lock.h>

/* RPC stuff */
#define RPC_VERS 2

typedef enum {
    RPC_CALL = 0,
    RPC_REPLY
} rpc_msg_type;

typedef enum {
    RPC_MSG_ACCEPTED = 0,
    RPC_MSG_DENIED
} rpc_reply_stat;

typedef enum {
    RPC_SUCCESS = 0,
    RPC_PROG_UNAVAIL,
    RPC_PROG_MISMATCH,
    RPC_PROC_UNAVAIL,
    RPC_GARBAGE_ARGS
} rpc_accept_stat;

typedef enum {
    RPC_MISMATCH = 0,
    RPC_AUTH_ERROR
} rpc_reject_stat;

typedef enum {
    RPC_AUTH_BADCRED = 0,
    RPC_AUTH_REJECTEDCRED,
    RPC_AUTH_BADVERF,
    RPC_AUTH_REJECTEDVERF,
    RPC_AUTH_TOOWEAK
} rpc_auth_stat;

typedef enum {
    RPC_AUTH_NULL = 0,
    RPC_AUTH_UNIX,
    RPC_AUTH_SHORT,
    RPC_AUTH_DES
} rpc_auth_flavor;

struct auth {
    rpc_auth_flavor auth_flavor;
    unsigned int auth_len;
    unsigned int data[0];
};

struct msg_header {
    unsigned int xid;
    rpc_msg_type msg_type;
};

struct call_body {
    unsigned int rpcvers;
    unsigned int prog;
    unsigned int vers;
    unsigned int proc;

    // insert auth here
};

/* portmapper stuff */
/* RFC 1057 */
#define RPC_PMAP_PORT 111
#define RPC_PMAP_PROG 100000
#define RPC_PMAP_VERS 2

struct rpc_pmap_mapping {
    unsigned int prog;
    unsigned int vers;
    unsigned int prot;
    unsigned int port;
};

enum rpc_pmap_proc {
    PMAPPROC_NULL = 0,
    PMAPPROC_SET = 1,
    PMAPPROC_UNSET = 2,
    PMAPPROC_GETPORT = 3,
    PMAPPROC_DUMP = 4,
    PMAPPROC_CALLIT = 5,
};

/* rpc api */
#define RPC_BUF_LEN 8192

typedef struct rpc_state {
    mutex lock;
    sockaddr server_addr;
    sock_id socket;
    unsigned int auth_cookie;
    unsigned char buf[RPC_BUF_LEN];
} rpc_state;

int rpc_init_state(rpc_state *state);
int rpc_destroy_state(rpc_state *state);
sock_id rpc_open_socket(rpc_state *state, const netaddr *server_addr);
int rpc_set_port(rpc_state *state, int port);
int rpc_call(rpc_state *state, unsigned int prog, unsigned int vers, unsigned int proc,
             const void *out_data, int out_data_len, void *in_data, int in_data_len);

int rpc_pmap_lookup(const netaddr *server_addr, unsigned int prog, unsigned int vers, unsigned int prot, int *port);

#endif
