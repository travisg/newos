/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/vfs.h>
#include <kernel/cbuf.h>
#include <kernel/heap.h>
#include <kernel/net/net.h>
#include <kernel/net/if.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/arp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/udp.h>
#include <kernel/net/socket.h>
#include <kernel/net/misc.h>
#include <boot/stage2.h>
#include <libc/string.h>

thread_id rx_thread_id;
int net_fd;

static int net_test_thread(void *unused)
{
	sock_id id;
	sockaddr addr;

	thread_snooze(2000000);

	memset(&addr, 0, sizeof(addr));
	addr.addr.len = 4;
	addr.addr.type = ADDR_TYPE_IP;
	addr.port = 9999;
	id = socket_create(SOCK_PROTO_UDP, 9999, &addr);
	dprintf("net_test_thread: id %d\n", id);

	for(;;) {
		ssize_t bytes_read;
		char buf[64];

		bytes_read = socket_read(id, buf, sizeof(buf));
		dprintf("net_test_thread: read %d bytes: '%s'\n", bytes_read, buf);
	}
}

int net_init(kernel_args *ka)
{
	int err;
	ifnet *i;
	ifaddr *addr;

	dprintf("net_init: entry\n");

	if_init();
	ethernet_init();
	arp_init();
	ipv4_init();
	udp_init();
	socket_init();

	// open the network device
	net_fd = sys_open("/dev/net/rtl8139/0", STREAM_TYPE_DEVICE, 0);
	if(net_fd < 0) {
		dprintf("net_init: no net devices\n");
		return -1;
	}

	// register the net device with the stack
	i = if_register_interface("/dev/net/rtl8139/0", IF_TYPE_ETHERNET);
	addr = kmalloc(sizeof(ifaddr));
	addr->addr.len = 6;
	addr->addr.type = ADDR_TYPE_ETHERNET;
	sys_ioctl(net_fd, 10000, &addr->addr.addr[0], 6);
	addr->broadcast.type = ADDR_TYPE_NULL;
	addr->netmask.type = ADDR_TYPE_NULL;
	if_bind_link_address(i, addr);

	// set the ip address for this net interface
	addr = kmalloc(sizeof(ifaddr));
	addr->addr.len = 4;
	addr->addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(&addr->addr) = 0x0a000063; // 10.0.0.99
	addr->netmask.len = 4;
	addr->netmask.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(&addr->netmask) = 0xffffff00; // 255.255.255.0
	addr->broadcast.len = 4;
	addr->broadcast.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(&addr->broadcast) = 0x0a0000ff; // 10.0.0.255
	if_bind_address(i, addr);

	// set up an initial routing table
	ipv4_route_add(0x0a000000, 0xffffff00, 0x0a000063, i->id);
	ipv4_route_add_gateway(0x00000000, 0x00000000, 0x0a000063, i->id, 0xc0000001);

	sys_close(net_fd);

	if_boot_interface(i);


#if 1
	// start the test thread
{
	thread_id id;

	id = thread_create_kernel_thread("net tester", &net_test_thread, NULL);
	thread_resume_thread(id);
}
#endif
	return 0;
}


