/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/khash.h>
#include <kernel/lock.h>
#include <kernel/heap.h>
#include <kernel/vm.h>
#include <kernel/fs/devfs.h>
#include <kernel/net/socket.h>
#include <newos/socket_api.h>

typedef struct socket_dev {
	sock_id id;
} socket_dev;

static int socket_dev_open(dev_ident ident, dev_cookie *cookie)
{
	socket_dev *s;

	s = (socket_dev *)kmalloc(sizeof(socket_dev));
	if(!s)
		return ERR_NO_MEMORY;

	s->id = -1;

	*cookie = s;

	return 0;
}

static int socket_dev_close(dev_cookie cookie)
{
	socket_dev *s = (socket_dev *)cookie;

	if(s->id >= 0)
		return socket_close(s->id);
	else
		return 0;
}

static int socket_dev_freecookie(dev_cookie cookie)
{
	kfree(cookie);
	return 0;
}

static int socket_dev_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static int socket_dev_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	socket_dev *s = (socket_dev *)cookie;
	_socket_api_args_t args;
	int err;

	// copy the args over from user space
	if((addr)buf >= KERNEL_BASE && (addr)buf <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;
	err = user_memcpy(&args, buf, sizeof(args));
	if(err < 0)
		return err;

	if(s->id < 0) {
		switch(op) {
			case _SOCKET_API_CREATE:
				err = s->id = socket_create(args.u.create.type, args.u.create.flags);
				break;
			default:
				err = ERR_INVALID_ARGS;
		}
	} else {
		switch(op) {
			case _SOCKET_API_BIND:
				err = socket_bind(s->id, args.u.bind.saddr);
				break;
			case _SOCKET_API_CONNECT:
				err = socket_connect(s->id, args.u.bind.saddr);
				break;
			case _SOCKET_API_RECVFROM:
				err = socket_recvfrom(s->id, args.u.transfer.buf, args.u.transfer.len, args.u.transfer.saddr);
				break;
			case _SOCKET_API_RECVFROM_ETC:
				err = socket_recvfrom_etc(s->id, args.u.transfer.buf, args.u.transfer.len, args.u.transfer.saddr, args.u.transfer.flags, args.u.transfer.timeout);
				break;
			case _SOCKET_API_SENDTO:
				err = socket_sendto(s->id, args.u.transfer.buf, args.u.transfer.len, args.u.transfer.saddr);
				break;
			default:
				err = ERR_INVALID_ARGS;
		}
	}

	return err;
}

static ssize_t socket_dev_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	socket_dev *s = (socket_dev *)cookie;

	if(s->id >= 0)
		return socket_read(s->id, buf, len);
	else
		return ERR_NET_NOT_CONNECTED;
}

static ssize_t socket_dev_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	socket_dev *s = (socket_dev *)cookie;

	if(s->id >= 0)
		return socket_write(s->id, buf, len);
	else
		return ERR_NET_NOT_CONNECTED;
}

static struct dev_calls socket_dev_hooks = {
	&socket_dev_open,
	&socket_dev_close,
	&socket_dev_freecookie,
	&socket_dev_seek,
	&socket_dev_ioctl,
	&socket_dev_read,
	&socket_dev_write,
	/* no paging from /dev/null */
	NULL,
	NULL,
	NULL
};

int socket_dev_init(void)
{
	// create device node
	devfs_publish_device("net/socket", NULL, &socket_dev_hooks);

	return 0;
}

