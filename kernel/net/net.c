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
#include <kernel/net/misc.h>
#include <boot/stage2.h>

thread_id rx_thread_id;
int net_fd;

int net_init(kernel_args *ka)
{
	int err;
	ifnet *i;
	ifaddr *addr;

	dprintf("net_init: entry\n");

	if_init();
	arp_init();
	ethernet_init();

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
	if_bind_link_address(i, addr);

	// set the ip address for this net interface
	addr = kmalloc(sizeof(ifaddr));
	addr->addr.len = 4;
	addr->addr.type = ADDR_TYPE_IP;
//	*(ipv4_addr *)&addr->addr.addr[0] = 0xc0a80063; // 192.168.0.99
	*(ipv4_addr *)&addr->addr.addr[0] = 0x0a000063; // 10.0.0.99
	if_bind_address(i, addr);

	rx_thread_id = thread_create_kernel_thread("net_rx_thread", &if_rx_thread, i);
	if(rx_thread_id < 0)
		panic("net_init: error creating rx_thread\n");
	i->rx_thread = rx_thread_id;
	thread_set_priority(rx_thread_id, THREAD_HIGH_PRIORITY);
	thread_resume_thread(rx_thread_id);

	sys_close(net_fd);

	return 0;
}


