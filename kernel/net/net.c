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
#include <kernel/net/loopback.h>
#include <kernel/net/arp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/udp.h>
#include <kernel/net/socket.h>
#include <kernel/net/misc.h>
#include <boot/stage2.h>
#include <libc/string.h>

thread_id rx_thread_id;
int net_fd;

static int net_test_thread2(void *unused)
{
	sock_id id;
	sockaddr addr;
	sockaddr saddr;
	char buf[64];

	thread_snooze(2000000);

	memset(&addr, 0, sizeof(addr));
	addr.addr.len = 4;
	addr.addr.type = ADDR_TYPE_IP;
	addr.port = 9998;
	id = socket_create(SOCK_PROTO_UDP, 9998, &addr);
	dprintf("net_test_thread: id %d\n", id);

	saddr.port = 9999;
	saddr.addr.len = 4;
	saddr.addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(&saddr.addr) = 0x7f000001; // loopback

	strcpy(buf, "loopback test");

	for(;;) {
		ssize_t bytes_written;

		// send it back
		bytes_written = socket_sendto(id, buf, strlen(buf), &saddr);
	}
}

static int net_test_thread3(void *unused)
{
	sock_id id;
	sockaddr addr;
	char buf[64];

	thread_snooze(2000000);

	memset(&addr, 0, sizeof(addr));
	addr.addr.len = 4;
	addr.addr.type = ADDR_TYPE_IP;
	addr.port = 9999;
	id = socket_create(SOCK_PROTO_UDP, 9999, &addr);
	dprintf("net_test_thread: id %d\n", id);

	for(;;) {
		ssize_t bytes_read;
		sockaddr saddr;
		char buf[64];

		bytes_read = socket_recvfrom(id, buf, sizeof(buf), &saddr);
		dprintf("net_test_thread3: read %ld bytes from host 0x%x, port %d: '%s'\n",
			bytes_read, NETADDR_TO_IPV4(&saddr.addr), saddr.port, buf);
	}
}

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
		ssize_t bytes_written;
		sockaddr saddr;
		char buf[64];

		bytes_read = socket_recvfrom(id, buf, sizeof(buf), &saddr);
		dprintf("net_test_thread: read %ld bytes from host 0x%x, port %d: '%s'\n",
			bytes_read, NETADDR_TO_IPV4(&saddr.addr), saddr.port, buf);

		// send it back
		bytes_written = socket_sendto(id, buf, bytes_read, &saddr);
	}
}

int net_init(kernel_args *ka)
{
	int err;
	ifnet *i;
	ifaddr *address;

	dprintf("net_init: entry\n");

	if_init();
	ethernet_init();
	arp_init();
	ipv4_init();
	loopback_init();
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
	if(!i) {
		dprintf("error allocating interface structure\n");
		return -1;
	}
	address = kmalloc(sizeof(ifaddr));
	address->addr.len = 6;
	address->addr.type = ADDR_TYPE_ETHERNET;
	sys_ioctl(net_fd, 10000, &address->addr.addr[0], 6);
	address->broadcast.len = 6;
	address->broadcast.type = ADDR_TYPE_ETHERNET;
	memset(&address->broadcast.addr[0], 0xff, 6);
	address->netmask.type = ADDR_TYPE_NULL;
	if_bind_link_address(i, address);

	// set the ip address for this net interface
	address = kmalloc(sizeof(ifaddr));
	address->addr.len = 4;
	address->addr.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(&address->addr) = 0x0a000063; // 10.0.0.99
	address->netmask.len = 4;
	address->netmask.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(&address->netmask) = 0xffffff00; // 255.255.255.0
	address->broadcast.len = 4;
	address->broadcast.type = ADDR_TYPE_IP;
	NETADDR_TO_IPV4(&address->broadcast) = 0x0a0000ff; // 10.0.0.255
	if_bind_address(i, address);

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
#if 0
	// start the other test threads
{
	thread_id id;

	id = thread_create_kernel_thread("net tester2", &net_test_thread2, NULL);
	thread_resume_thread(id);
	id = thread_create_kernel_thread("net tester3", &net_test_thread3, NULL);
	thread_resume_thread(id);
}
#endif
	return 0;
}


