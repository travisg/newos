/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <socket/socket.h>
#include <unistd.h>
#include <sys/syscalls.h>

int socket_create(int proto, int flags)
{
	int fd;
	int err;
	_socket_api_args_t args;

	fd = open("/dev/net/socket", 0);
	if(fd < 0)
		return fd;

	args.u.create.type = proto;
	args.u.create.flags = flags;

	err = ioctl(fd, _SOCKET_API_CREATE, &args, sizeof(args));
	if(err < 0) {
		close(fd);
		return err;
	}

	return fd;
}

int socket_close(int fd)
{
	return close(fd);
}

int socket_bind(int fd, sockaddr *addr)
{
	_socket_api_args_t args;

	args.u.bind.saddr = addr;
	return ioctl(fd, _SOCKET_API_BIND, &args, sizeof(args));
}

int socket_connect(int fd, sockaddr *addr)
{
	_socket_api_args_t args;

	args.u.connect.saddr = addr;
	return ioctl(fd, _SOCKET_API_CONNECT, &args, sizeof(args));
}

int socket_listen(int fd)
{
	return ioctl(fd, _SOCKET_API_LISTEN, NULL, 0);
}

int socket_accept(int fd, sockaddr *addr)
{
	_socket_api_args_t args;

	args.u.accept.saddr = addr;
	return ioctl(fd, _SOCKET_API_ACCEPT, &args, sizeof(args));
}

ssize_t socket_read(int fd, void *buf, ssize_t len)
{
	return read(fd, buf, len);
}

ssize_t socket_write(int fd, const void *buf, ssize_t len)
{
	return write(fd, buf, len);
}

ssize_t socket_recvfrom(int fd, void *buf, ssize_t len, sockaddr *addr)
{
	_socket_api_args_t args;

	args.u.transfer.buf = buf;
	args.u.transfer.len = len;
	args.u.transfer.flags = 0;
	args.u.transfer.saddr = addr;
	args.u.transfer.timeout = 0;

	return ioctl(fd, _SOCKET_API_RECVFROM, &args, sizeof(args));
}

ssize_t socket_recvfrom_etc(int fd, void *buf, ssize_t len, sockaddr *addr, int flags, bigtime_t timeout)
{
	_socket_api_args_t args;

	args.u.transfer.buf = buf;
	args.u.transfer.len = len;
	args.u.transfer.flags = flags;
	args.u.transfer.saddr = addr;
	args.u.transfer.timeout = timeout;

	return ioctl(fd, _SOCKET_API_RECVFROM_ETC, &args, sizeof(args));
}

ssize_t socket_sendto(int fd, const void *buf, ssize_t len, sockaddr *addr)
{
	_socket_api_args_t args;

	args.u.transfer.buf = (void *)buf;
	args.u.transfer.len = len;
	args.u.transfer.flags = 0;
	args.u.transfer.saddr = addr;
	args.u.transfer.timeout = 0;

	return ioctl(fd, _SOCKET_API_SENDTO, &args, sizeof(args));
}

