/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/thread.h>
#include <kernel/vfs.h>
#include <kernel/net/net.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ipv4.h>
#include <boot/stage2.h>

thread_id rx_thread_id;
int net_fd;

uint8 buf[2048];

int rx_thread()
{
	int err;
	
	if(net_fd < 0)
		return -1;

	for(;;) {
		size_t len = sizeof(buf);

		err = sys_read(net_fd, buf, 0, &len);
		if(err < 0) {
			thread_snooze(10000);
			continue;
		}
		if(len == 0)
			continue;

		ethernet_receive(buf, 0, len);
	}

	return 0;
}

int net_init(kernel_args *ka)
{
	int err;

	dprintf("net_init: entry\n");

	// open the network device
	net_fd = sys_open("/dev/net/rtl8139/0", "", STREAM_TYPE_DEVICE);
	if(net_fd < 0) {
		dprintf("net_init: no net devices\n");
		return -1;
	}

	// set the station's ip address
	// XXX hard coded now
	ipv4_set_station_address(0x0a000002); // 10.0.0.2

	{
		ethernet_addr eaddr;

		sys_ioctl(net_fd, 10000, &eaddr, sizeof(eaddr));
		ethernet_set_station_addr(eaddr);
	}

	rx_thread_id = thread_create_kernel_thread("net_rx_thread", &rx_thread, 64);
	if(rx_thread_id < 0)
		panic("net_init: error creating rx_thread\n");
	thread_resume_thread(rx_thread_id);

	return 0;
}


