/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <newos/errors.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <socket/socket.h>

static int open_listen_socket(int port)
{
	int err;
	sockaddr addr;
	int fd;

	fd = socket_create(SOCK_PROTO_TCP, 0);
	if(fd < 0)
		return fd;

	memset(&addr, 0, sizeof(addr));
	addr.addr.len = 4;
	addr.addr.type = ADDR_TYPE_IP;
	addr.port = port;
	NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(0,0,0,0);

	err = socket_bind(fd, &addr);
	if(err < 0)
		goto err;

	err = socket_listen(fd);
	if(err < 0)
		goto err;

	return fd;

err:
	socket_close(fd);

	return err;
}

int main(int argc, char **argv)
{
	int port;
	int listenfd;
	char **spawn_argv;
	int spawn_argc;
	int i;

	if(argc < 3) {
		printf("%s: not enough arguments\n", argv[0]);
		return -1;
	}

	port = atoi(argv[1]);
	if(port == 0) {
		printf("%s: invalid port number\n", argv[0]);
		return -1;
	}

	// make us be our own session
	setsid();

	// build an array of args to pass anything we start up
	spawn_argc = argc - 2;
	spawn_argv = (char **)malloc(sizeof(char *) * spawn_argc);
	if(spawn_argv == NULL)
		return -1;
	for(i = 0; i < spawn_argc; i++) {
		spawn_argv[i] = argv[i + 2];
	}

	listenfd = open_listen_socket(port);
	if(listenfd < 0)
		return listenfd;

	for(;;) {
		int new_fd;
		sockaddr addr;
		int saved_stdin, saved_stdout, saved_stderr;
		proc_id pid;

		new_fd = socket_accept(listenfd, &addr);
		if(new_fd < 0)
			continue;

		saved_stdin = dup(0);
		saved_stdout = dup(1);
		saved_stderr = dup(2);

		dup2(new_fd, 0);
		dup2(new_fd, 1);
		dup2(new_fd, 2);
		close(new_fd);

		pid = _kern_proc_create_proc(spawn_argv[0], spawn_argv[0], spawn_argv, spawn_argc, 5);

		if(pid > 0) {
			// give the spawned process a new process group
			setpgid(pid, 0);
		}

		dup2(saved_stdin, 0);
		dup2(saved_stdout, 1);
		dup2(saved_stderr, 2);
		close(saved_stdin);
		close(saved_stdout);
		close(saved_stderr);
	}

	return 0;
}

