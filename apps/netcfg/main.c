/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <newos/errors.h>
#include <newos/net.h>
#include <sys/syscalls.h>

static int usage(const char *argv[])
{
	printf("usage:\n");
	printf("%s\n", argv[0]);
	printf("\tif create <interface>\n");
	printf("\tif delete <interface>\n");
	printf("\tif addaddr <interface> ipv4 addr <ip address> mask <netmask> broadcast <broadcast>\n");
	printf("\tif rmaddr  <interface> ipv4 addr <ip address>\n");
	printf("\tif list\n");
	printf("\n");
	printf("\troute add default ipv4 addr <ip address> if <interface> ipv4 addr <ip address>\n");
	printf("\troute add net ipv4 addr <ip address> mask <netmask> if <interface> ipv4 addr <ip address>\n");
	printf("\troute delete ipv4 addr <ip address>\n");
	printf("\troute list\n");

	return -1;
}

static int parse_ipv4_addr_string(ipv4_addr *ip_addr, const char *_ip_addr_string)
{
	int a, b;
	char ip_addr_string[128];

	strlcpy(ip_addr_string, _ip_addr_string, sizeof(ip_addr_string));

	*ip_addr = 0;

	// walk through the first number
	a = 0;
	b = 0;
	for(; ip_addr_string[b] != 0 && ip_addr_string[b] != '.'; b++)
		;
	if(ip_addr_string[b] == 0)
		return ERR_NOT_FOUND;
	ip_addr_string[b] = 0;
	*ip_addr = atoi(&ip_addr_string[a]) << 24;
	b++;

	// second digit
	a = b;
	for(; ip_addr_string[b] != 0 && ip_addr_string[b] != '.'; b++)
		;
	if(ip_addr_string[b] == 0)
		return ERR_NOT_FOUND;
	ip_addr_string[b] = 0;
	*ip_addr |= atoi(&ip_addr_string[a]) << 16;
	b++;

	// third digit
	a = b;
	for(; ip_addr_string[b] != 0 && ip_addr_string[b] != '.'; b++)
		;
	if(ip_addr_string[b] == 0)
		return ERR_NOT_FOUND;
	ip_addr_string[b] = 0;
	*ip_addr |= atoi(&ip_addr_string[a]) << 8;
	b++;

	// last digit
	a = b;
	for(; ip_addr_string[b] != 0 && ip_addr_string[b] != '.'; b++)
		;
	ip_addr_string[b] = 0;
	*ip_addr |= atoi(&ip_addr_string[a]);

	return NO_ERROR;
}

static int do_if(int argc, const char *argv[], int curr_arg)
{
	int op;
	const char *interface_name = NULL;
	ipv4_addr if_addr = 0;
	ipv4_addr mask_addr = 0;
	ipv4_addr broadcast_addr = 0;
	struct _ioctl_net_if_control_struct control;

	if(curr_arg >= argc)
		return usage(argv);

	/* parse the args for the various commands */
	if(!strncasecmp(argv[curr_arg], "create", sizeof("create"))) {
		if(curr_arg + 1 >= argc)
			return usage(argv);
		op = IOCTL_NET_CONTROL_IF_CREATE;
		interface_name = argv[curr_arg + 1];
	} else if(!strncasecmp(argv[curr_arg], "delete", sizeof("delete"))) {
		if(curr_arg + 1 >= argc)
			return usage(argv);
		op = IOCTL_NET_CONTROL_IF_DELETE;
		interface_name = argv[curr_arg + 1];
	} else if(!strncasecmp(argv[curr_arg], "addaddr", sizeof("addaddr"))) {
		if(curr_arg + 8 >= argc)
			return usage(argv);
		op = IOCTL_NET_CONTROL_IF_ADDADDR;
		interface_name = argv[curr_arg + 1];
		if(strncasecmp(argv[curr_arg + 2], "ipv4", sizeof("ipv4")) ||
			strncasecmp(argv[curr_arg + 3], "addr", sizeof("addr")) ||
			strncasecmp(argv[curr_arg + 5], "mask", sizeof("mask")) ||
			strncasecmp(argv[curr_arg + 7], "broadcast", sizeof("broadcast")))
			return usage(argv);
		parse_ipv4_addr_string(&if_addr, argv[curr_arg + 4]);
		parse_ipv4_addr_string(&mask_addr, argv[curr_arg + 6]);
		parse_ipv4_addr_string(&broadcast_addr, argv[curr_arg + 8]);
	} else if(!strncasecmp(argv[curr_arg], "rmaddr", sizeof("rmaddr"))) {
		if(curr_arg + 4 >= argc)
			return usage(argv);
		op = IOCTL_NET_CONTROL_IF_RMADDR;
		interface_name = argv[curr_arg + 1];
		if(strncasecmp(argv[curr_arg + 2], "ipv4", sizeof("ipv4")) ||
			strncasecmp(argv[curr_arg + 3], "addr", sizeof("addr")))
			return usage(argv);
		parse_ipv4_addr_string(&if_addr, argv[curr_arg + 4]);
	} else if(!strncasecmp(argv[curr_arg], "list", sizeof("list"))) {
		op = IOCTL_NET_CONTROL_IF_LIST;
	} else {
		return usage(argv);
	}

//	printf("do_if: op %d, interface_name '%s', if_addr 0x%x, mask_addr 0x%x, broadcast_addr 0x%x\n",
//		op, interface_name, if_addr, mask_addr, broadcast_addr);

	// fill out the control structure
	memset(&control, 0, sizeof(control));
	control.if_name[0] = 0;
	if(interface_name)
		strlcpy(control.if_name, interface_name, sizeof(control.if_name));
	control.if_addr.len = 4;
	control.if_addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(control.if_addr) = if_addr;
	control.mask_addr.len = 4;
	control.mask_addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(control.mask_addr) = mask_addr;
	control.broadcast_addr.len = 4;
	control.broadcast_addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(control.broadcast_addr) = broadcast_addr;

	// call the ioctl
	{
		int fd;
		int err;

		fd = sys_open(NET_CONTROL_DEV, STREAM_TYPE_DEVICE, 0);
		if(fd < 0) {
			printf("error opening network control device\n");
			return fd;
		}
		err = sys_ioctl(fd, op, &control, sizeof(control));
		sys_close(fd);

		if(err < 0) {
			printf("error calling ioctl %d (%s)\n", err, strerror(err));
		}

//		printf("do_if: ioctl returned %d\n", err);
		return err;
	}
}

static int do_route(int argc, const char *argv[], int curr_arg)
{
	int op;
	ipv4_addr net_addr = 0;
	ipv4_addr mask_addr = 0;
	ipv4_addr if_addr = 0;
	const char *interface_name = NULL;
	struct _ioctl_net_route_struct control;

	if(curr_arg >= argc)
		return usage(argv);

	/* parse the args for the various commands */
	if(!strncasecmp(argv[curr_arg], "add", sizeof("add"))) {
		curr_arg++;
		if(curr_arg >= argc)
			return usage(argv);
		if(!strncasecmp(argv[curr_arg], "default", sizeof("default"))) {
			if(curr_arg + 8 >= argc)
				return usage(argv);
			if(strncasecmp(argv[curr_arg + 1], "ipv4", sizeof("ipv4")) ||
				strncasecmp(argv[curr_arg + 2], "addr", sizeof("addr")) ||
				strncasecmp(argv[curr_arg + 4], "if", sizeof("if")) ||
				strncasecmp(argv[curr_arg + 6], "ipv4", sizeof("ipv4")) ||
				strncasecmp(argv[curr_arg + 7], "addr", sizeof("addr")))
				return usage(argv);
			op = IOCTL_NET_CONTROL_ROUTE_ADD;
			parse_ipv4_addr_string(&net_addr, argv[curr_arg + 3]);
			interface_name = argv[curr_arg + 5];
			parse_ipv4_addr_string(&if_addr, argv[curr_arg + 8]);
		} else if(!strncasecmp(argv[curr_arg], "net", sizeof("net"))) {
			if(curr_arg + 10 >= argc)
				return usage(argv);
			if(strncasecmp(argv[curr_arg + 1], "ipv4", sizeof("ipv4")) ||
				strncasecmp(argv[curr_arg + 2], "addr", sizeof("addr")) ||
				strncasecmp(argv[curr_arg + 4], "mask", sizeof("mask")) ||
				strncasecmp(argv[curr_arg + 6], "if", sizeof("if")) ||
				strncasecmp(argv[curr_arg + 8], "ipv4", sizeof("ipv4")) ||
				strncasecmp(argv[curr_arg + 9], "addr", sizeof("addr")))
				return usage(argv);
			op = IOCTL_NET_CONTROL_ROUTE_ADD;
			parse_ipv4_addr_string(&net_addr, argv[curr_arg + 3]);
			parse_ipv4_addr_string(&mask_addr, argv[curr_arg + 5]);
			interface_name = argv[curr_arg + 7];
			parse_ipv4_addr_string(&if_addr, argv[curr_arg + 10]);
		} else {
			return usage(argv);
		}
	} else if(!strncasecmp(argv[curr_arg], "delete", sizeof("delete"))) {
		if(curr_arg + 3 >= argc)
			return usage(argv);
		if(strncasecmp(argv[curr_arg + 1], "ipv4", sizeof("ipv4")) ||
			strncasecmp(argv[curr_arg + 2], "addr", sizeof("addr")))
			return usage(argv);
		op = IOCTL_NET_CONTROL_ROUTE_DELETE;
		parse_ipv4_addr_string(&net_addr, argv[curr_arg + 3]);
	} else if(!strncasecmp(argv[curr_arg], "list", sizeof("list"))) {
		op = IOCTL_NET_CONTROL_ROUTE_LIST;
	} else {
		return usage(argv);
	}

//	printf("do_route: op %d, net_addr 0x%x, mask_addr 0x%x, if_addr 0x%x, interface_name '%s'\n",
//		op, net_addr, mask_addr, if_addr, interface_name);

	// fill out the control structure
	memset(&control, 0, sizeof(control));
	control.net_addr.len = 4;
	control.net_addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(control.net_addr) = net_addr;
	control.if_addr.len = 4;
	control.if_addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(control.if_addr) = if_addr;
	control.mask_addr.len = 4;
	control.mask_addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(control.mask_addr) = mask_addr;
	control.if_name[0] = 0;
	if(interface_name)
		strlcpy(control.if_name, interface_name, sizeof(control.if_name));

	// call the ioctl
	{
		int fd;
		int err;

		fd = sys_open(NET_CONTROL_DEV, STREAM_TYPE_DEVICE, 0);
		if(fd < 0) {
			printf("error opening network control device\n");
			return fd;
		}
		err = sys_ioctl(fd, op, &control, sizeof(control));
		sys_close(fd);

		if(err < 0) {
			printf("error calling ioctl %d (%s)\n", err, strerror(err));
		}

//		printf("do_if: ioctl returned %d\n", err);
		return err;
	}

	return 0;
}

int main(int argc, const char *argv[])
{
	int curr_arg;

	/* start processing args */
	if(argc < 2)
		return usage(argv);

	curr_arg = 1;
	if(!strncasecmp(argv[curr_arg], "if", sizeof("if"))) {
		return do_if(argc, argv, curr_arg + 1);
	} else if(!strncasecmp(argv[curr_arg], "route", sizeof("route"))) {
		return do_route(argc, argv, curr_arg + 1);
	} else {
		return usage(argv);
	}

	return 0;
}

