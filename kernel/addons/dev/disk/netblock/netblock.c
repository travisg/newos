/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/thread.h>
#include <kernel/lock.h>
#include <kernel/fs/devfs.h>
#include <kernel/net/socket.h>
#include <kernel/net/misc.h>
#include <kernel/net/ipv4.h>
#include <newos/errors.h>
#include <string.h>
#include <stdio.h>

#define NB_TRACE 1
#if NB_TRACE
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif

#define BLOCKSIZE 512

struct netblock_dev {
	sock_id sockfd;
	mutex lock;
	off_t size;
	sockaddr saddr;
};

struct netblock_cookie {
	struct netblock_dev *dev;
	off_t curr_pos;
};

#define MAGIC 0x14983543

typedef struct message {
	int magic;
	int block_id;
	int command;
	int arg;
	int arg2;
	char data[0];
} message;

#define CMD_NULL 0
#define CMD_GETSIZE 1
#define CMD_READBLOCK 2
#define CMD_WRITEBLOCK 3

// unused
#if 0
static void dump_message(message *msg)
{
	dprintf("message:\n");
	dprintf(" magic: 0x%x\n", ntohl(msg->magic));
	dprintf(" block_id: %d\n", ntohl(msg->block_id));
	dprintf(" command: %d\n", ntohl(msg->command));
	dprintf(" arg: %d\n", ntohl(msg->arg));
	dprintf(" arg2: %d\n", ntohl(msg->arg2));
}
#endif

static int validate_message(message *msg, int len)
{
	if(len < (int)sizeof(message))
		return 0;

	// validate the message
	if(ntohl(msg->magic) != MAGIC)
		return 0;
	if(ntohl(msg->block_id) != 0)
		return 0;

	switch(ntohl(msg->command)) {
		case CMD_NULL:
		case ~CMD_NULL:
		case CMD_GETSIZE:
		case ~CMD_GETSIZE:
		case CMD_READBLOCK:
		case ~CMD_READBLOCK:
		case CMD_WRITEBLOCK:
		case ~CMD_WRITEBLOCK:
			break;
		default:
			return 0;
	}

	return 1;
}

static void init_message(message *msg, int command)
{
	msg->magic = htonl(MAGIC);
	msg->block_id = htonl(0);
	msg->command = htonl(command);
	msg->arg = 0;
	msg->arg2 = 0;
}

static int send_message(struct netblock_dev *dev, message *msg, ssize_t len, void *recv_buf, int recv_len)
{
	int err;
	sockaddr fromaddr;
	message *rmsg = (message *)recv_buf;

retry:
	err = socket_sendto(dev->sockfd, msg, len, &dev->saddr);
	if(err < 0)
		return err;

	err = socket_recvfrom_etc(dev->sockfd, recv_buf, recv_len, &fromaddr, SOCK_FLAG_TIMEOUT, 250000); // 1/4 sec
	if(err < 0)
		goto retry;

	// look at the message and see if it's valid
	if(!validate_message(rmsg, err))
		goto retry;

	// wait for the valid response
	if(rmsg->command != ~msg->command)
		goto retry;

	return NO_ERROR;
}

static off_t get_device_size(struct netblock_dev *dev)
{
	message msg;
	char buf[4096];
	message *rmsg;

	init_message(&msg, CMD_GETSIZE);

	send_message(dev, &msg, sizeof(message), buf, sizeof(buf));
	rmsg = (message *)buf;

	return ntohl(*(int *)&rmsg->data[0]);
}

static int read_block(struct netblock_dev *dev, off_t offset, void *out_buf)
{
	message msg;
	char buf[sizeof(message) + BLOCKSIZE];
	message *rmsg = (message *)buf;

	init_message(&msg, CMD_READBLOCK);

	msg.arg = htonl(offset);
	msg.arg2 = htonl(BLOCKSIZE);

	send_message(dev, &msg, sizeof(message), buf, sizeof(buf));

	memcpy(out_buf, &rmsg->data[0], BLOCKSIZE);

	return 0;
}

static int write_block(struct netblock_dev *dev, off_t offset, const void *out_buf)
{
	message *msg;
	char buf[sizeof(message) + BLOCKSIZE];
	char buf2[sizeof(message) + BLOCKSIZE];

	msg = (message *)buf;

	init_message(msg, CMD_WRITEBLOCK);

	msg->arg = htonl(offset);
	msg->arg2 = htonl(BLOCKSIZE);

	memcpy(&msg->data[0], out_buf, BLOCKSIZE);

	send_message(dev, msg, sizeof(message) + BLOCKSIZE, buf2, sizeof(buf2));

	return 0;
}

static int netblock_open(dev_ident ident, dev_cookie *_cookie)
{
	struct netblock_cookie *cookie;

	cookie = kmalloc(sizeof(struct netblock_cookie));
	if(!cookie)
		return ERR_NO_MEMORY;

	cookie->dev = ident;
	cookie->curr_pos = 0;

	cookie->dev->size = get_device_size(cookie->dev);

	*_cookie = cookie;

	return 0;
}

static int netblock_freecookie(dev_cookie cookie)
{
	kfree(cookie);

	return 0;
}

static int netblock_seek(dev_cookie _cookie, off_t pos, seek_type st)
{
	struct netblock_cookie *cookie = _cookie;
	int err = NO_ERROR;

	TRACE(("netblock_seek: cookie %p, pos 0x%Lx, st %d\n", cookie, pos, st));

	mutex_lock(&cookie->dev->lock);

	switch(st) {
		case _SEEK_SET:
			if(pos > cookie->dev->size)
				pos = cookie->dev->size;
			if(pos < 0)
				pos = 0;
			cookie->curr_pos = pos;
			break;
		case _SEEK_CUR:
			if(cookie->curr_pos + pos > cookie->dev->size)
				cookie->curr_pos = cookie->dev->size;
			else if(cookie->curr_pos + pos < 0)
				cookie->curr_pos = 0;
			else
				cookie->curr_pos = cookie->curr_pos + pos;
			break;
		case _SEEK_END:
			if(cookie->dev->size - pos > cookie->dev->size)
				cookie->curr_pos = cookie->dev->size;
			else if(cookie->dev->size + pos < 0)
				cookie->curr_pos = 0;
			else
				cookie->curr_pos = cookie->dev->size - pos;
			break;
		default:
			err = ERR_INVALID_ARGS;
	}

	mutex_unlock(&cookie->dev->lock);

	return err;
}

static int netblock_close(dev_cookie cookie)
{
//	dprintf("netblock_close: entry\n");

	return 0;
}

static int netblock_ioctl(dev_cookie _cookie, int op, void *buf, size_t len)
{
	struct netblock_cookie *cookie = _cookie;
	int err;

	TRACE(("netblock_ioctl: cookie %p, op %d, buf %p, len %lu\n", cookie, op, buf, (unsigned long)len));

	mutex_lock(&cookie->dev->lock);

	err = NO_ERROR;
	switch(op) {
		case 90000: { // set the netblock server address
			sockaddr *saddr = (sockaddr *)buf;

			memcpy(&cookie->dev->saddr, saddr, sizeof(sockaddr));
			break;
		}
		case 90001: { // read the device's size
			cookie->dev->size = get_device_size(cookie->dev);
			dprintf("netblock_ioctl: device size is %Ld\n", cookie->dev->size);
			break;
		}
		default:
			err = ERR_INVALID_ARGS;
	}

	mutex_unlock(&cookie->dev->lock);

	return err;
}

static ssize_t _netblock_readwrite(struct netblock_dev *dev, bool write, void *_buf, off_t pos, ssize_t len)
{
	uint8 *buf = _buf;
	ssize_t bytes_transferred = 0;

	if(pos % BLOCKSIZE > 0){
		// partial first buffer
		ssize_t tomove = min(len, BLOCKSIZE - (pos % BLOCKSIZE));
		off_t blockaddr = ROUNDOWN(pos, BLOCKSIZE);
		int blockoff = pos % BLOCKSIZE;
		uint8 temp_buf[BLOCKSIZE];

		if(write) {
			read_block(dev, blockaddr, temp_buf);
			memcpy(&temp_buf[blockoff], buf, tomove);
			write_block(dev, blockaddr, temp_buf);
		} else {
			read_block(dev, blockaddr, temp_buf);
			memcpy(buf, &temp_buf[blockoff], tomove);
		}
		buf += tomove;
		len -= tomove;
		pos += tomove;
		bytes_transferred += tomove;
	}

	while(len >= BLOCKSIZE) {
		if(write)
			write_block(dev, pos, buf);
		else
			read_block(dev, pos, buf);
		buf += BLOCKSIZE;
		len -= BLOCKSIZE;
		pos += BLOCKSIZE;
		bytes_transferred += BLOCKSIZE;
	}

	if(len > 0) {
		// partial last buffer
		ssize_t tomove = min(len, BLOCKSIZE);
		off_t blockaddr = ROUNDOWN(pos, BLOCKSIZE);
		uint8 temp_buf[BLOCKSIZE];

		if(write) {
			read_block(dev, blockaddr, temp_buf);
			memcpy(temp_buf, buf, tomove);
			write_block(dev, blockaddr, temp_buf);
		} else {
			read_block(dev, blockaddr, temp_buf);
			memcpy(buf, temp_buf, tomove);
		}
		len -= tomove;
		bytes_transferred += tomove;
	}

	return bytes_transferred;
}

static ssize_t netblock_read(dev_cookie _cookie, void *_buf, off_t pos, ssize_t len)
{
	struct netblock_cookie *cookie = _cookie;
	ssize_t bytes_read = 0;
	bool update_pos = false;

	TRACE(("netblock_read: cookie %p, buf %p, pos 0x%Ld, len %lu\n", cookie, _buf, pos, (long)len));

	mutex_lock(&cookie->dev->lock);

	// trim the pos and len to be proper
	if(pos < 0) {
		pos = cookie->curr_pos;
		update_pos = true;
	}
	if(pos > cookie->dev->size) {
		bytes_read = 0;
		goto out;
	}
	if(len <= 0) {
		bytes_read = 0;
		goto out;
	}
	if(pos + len > cookie->dev->size) {
		len = cookie->dev->size - pos;
	}

	bytes_read = _netblock_readwrite(cookie->dev, false, _buf, pos, len);

	if(update_pos && bytes_read > 0)
		cookie->curr_pos += bytes_read;

out:
	mutex_unlock(&cookie->dev->lock);

	return bytes_read;
}

static ssize_t netblock_write(dev_cookie _cookie, const void *_buf, off_t pos, ssize_t len)
{
	struct netblock_cookie *cookie = _cookie;
	ssize_t bytes_written = 0;
	bool update_pos = false;

	TRACE(("netblock_write: cookie %p, buf %p, pos 0x%Ld, len %lu\n", cookie, _buf, pos, (long)len));

	mutex_lock(&cookie->dev->lock);

	// trim the pos and len to be proper
	if(pos < 0) {
		pos = cookie->curr_pos;
		update_pos = true;
	}
	if(pos > cookie->dev->size) {
		bytes_written = 0;
		goto out;
	}
	if(len <= 0) {
		bytes_written = 0;
		goto out;
	}
	if(pos + len > cookie->dev->size) {
		len = cookie->dev->size - pos;
	}

	bytes_written = _netblock_readwrite(cookie->dev, true, (void *)_buf, pos, len);

	if(update_pos && bytes_written > 0)
		cookie->curr_pos += bytes_written;

out:
	mutex_unlock(&cookie->dev->lock);

	return bytes_written;
}

static int netblock_canpage(dev_ident ident)
{
	return 1;
}

static ssize_t netblock_readpage(dev_ident ident, iovecs *vecs, off_t pos)
{
	struct netblock_dev *dev = ident;
	ssize_t bytes_read = 0;
	unsigned int curr_vec;

	TRACE(("netblock_readpage: dev %p, vecs %p, num %lu, total_len %lu, pos 0x%Ld\n", dev, vecs, (unsigned long)vecs->num, (unsigned long)vecs->total_len, pos));

	if(pos % BLOCKSIZE != 0)
		return ERR_INVALID_ARGS;

	mutex_lock(&dev->lock);

	// cycle through all of the iovecs
	for(curr_vec = 0; curr_vec < vecs->num; curr_vec++) {
		size_t vec_len = vecs->vec[curr_vec].len;
		size_t vec_offset = 0;

		// make sure this vec has a good len
		if(vec_len % BLOCKSIZE != 0)
			break;

		// read the blocks into the proper place
		while(vec_len > 0) {
			read_block(dev, pos, ((uint8 *)vecs->vec[curr_vec].start) + vec_offset);

			vec_offset += BLOCKSIZE;
			vec_len -= BLOCKSIZE;
			pos += BLOCKSIZE;
			bytes_read += BLOCKSIZE;
		}
	}

	mutex_unlock(&dev->lock);

	return bytes_read;
}

static ssize_t netblock_writepage(dev_ident ident, iovecs *vecs, off_t pos)
{
	struct netblock_dev *dev = ident;
	ssize_t bytes_written = 0;
	unsigned int curr_vec;

	TRACE(("netblock_writepage: dev %p, vecs %p, num %lu, total_len %lu, pos 0x%Ld\n", dev, vecs, (unsigned long)vecs->num, (unsigned long)vecs->total_len, pos));

	if(pos % BLOCKSIZE != 0)
		return ERR_INVALID_ARGS;

	mutex_lock(&dev->lock);

	// cycle through all of the iovecs
	for(curr_vec = 0; curr_vec < vecs->num; curr_vec++) {
		size_t vec_len = vecs->vec[curr_vec].len;
		size_t vec_offset = 0;

		// make sure this vec has a good len
		if(vec_len % BLOCKSIZE != 0)
			break;

		// read the blocks into the proper place
		while(vec_len > 0) {
			write_block(dev, pos, ((uint8 *)vecs->vec[curr_vec].start) + vec_offset);

			vec_offset += BLOCKSIZE;
			vec_len -= BLOCKSIZE;
			pos += BLOCKSIZE;
			bytes_written += BLOCKSIZE;
		}
	}

	mutex_unlock(&dev->lock);

	return bytes_written;
}

static struct dev_calls netblock_hooks = {
	&netblock_open,
	&netblock_close,
	&netblock_freecookie,
	&netblock_seek,
	&netblock_ioctl,
	&netblock_read,
	&netblock_write,
	&netblock_canpage,
	&netblock_readpage,
	&netblock_writepage
};

int dev_bootstrap(void);

int dev_bootstrap(void)
{
	struct netblock_dev *dev;
	sockaddr saddr;

	int index = 0;
	char path[SYS_MAX_PATH_LEN];

	// create a device structure
	dev = kmalloc(sizeof(struct netblock_dev));
	if(!dev)
		return -1;

	// create the socket
	saddr.port = 6789;
	saddr.addr.len = 4;
	saddr.addr.type = ADDR_TYPE_IP;
	memset(&saddr.addr.addr[0], 0, 4);
	dev->sockfd = socket_create(SOCK_PROTO_UDP, 0);
	if(dev->sockfd < 0) {
		dprintf("netblock_dev_init: error creating socket\n");
		kfree(dev);
		return -1;
	}
	socket_bind(dev->sockfd, &saddr);

	mutex_init(&dev->lock, "netblock_lock");
	dev->size = -1;

	dev->saddr.port = 9999;
	dev->saddr.addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(dev->saddr.addr) = IPV4_DOTADDR_TO_ADDR(192, 168, 0, 3);

	index = devfs_publish_indexed_directory("disk/netblock");
	if (index >= 0) {
		sprintf(path, "disk/netblock/%d/raw", index);
		devfs_publish_device(path, dev, &netblock_hooks);
	}

	return 0;
}

