/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <newos/errors.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <socket/socket.h>

int fd;

int read_thread(void *args)
{
	char buf[1024];
	ssize_t len;
	int i;

	for(;;) {
		len = socket_read(fd, buf, sizeof(buf));
		if(len < 0) {
			printf("\nsocket_read returns %d\n", len);
			break;
		}

		for(i=0; i<len; i++)
			printf("%c", buf[i]);
	}

	return 0;
}

int write_thread(void *args)
{
	char c;

	for(;;) {
		c = getchar();
		socket_write(fd, &c, sizeof(c));
	}
	return 0;
}

int main(int argc, char **argv)
{
	int err;
	sockaddr addr;
	int i;

	fd = socket_create(SOCK_PROTO_TCP, 0);
	printf("created socket, fd %d\n", fd);
	if(fd < 0)
		return 0;

	memset(&addr, 0, sizeof(addr));
	addr.addr.len = 4;
	addr.addr.type = ADDR_TYPE_IP;
//	addr.port = 19;
//	NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(63,203,215,73);
//	addr.port = 23;
//	NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(192,168,0,3);
	addr.port = 6667;
	NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(209,131,227,242);

	err = socket_connect(fd, &addr);
	printf("socket_connect returns %d\n", err);
	if(err < 0)
		return err;

	sys_thread_resume_thread(sys_thread_create_thread("read_thread", &read_thread, NULL));
	sys_thread_resume_thread(sys_thread_create_thread("write_thread", &write_thread, NULL));

	for(;;)
		sys_snooze(1000000);

	return 0;
}


