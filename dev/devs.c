/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <dev/devs.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>

#include <libc/string.h>

#ifdef ARCH_i386
#include <dev/arch/i386/pci/pci_bus.h>
#include <dev/arch/i386/console/console_dev.h>
#endif
#ifdef ARCH_sh4
#include <dev/arch/sh4/maple/maple_bus.h>
#include <dev/arch/sh4/console/console_dev.h>
#include <dev/arch/sh4/rtl8139/rtl8139_dev.h>
#endif
#include <dev/common/null.h>
#include <dev/common/zero.h>

int devs_init(kernel_args *ka)
{

	null_dev_init(ka);
	zero_dev_init(ka);
#ifdef ARCH_i386
	pci_bus_init(ka);
	console_dev_init(ka);
#endif	

#ifdef ARCH_sh4
	maple_bus_init(ka);
	console_dev_init(ka);
	rtl8139_dev_init(ka);
#endif

#if 0
	{
		int fd;
		
		dprintf("devs_init: devfs test:\n");
		fd = vfs_open(NULL, "/dev", "", STREAM_TYPE_DIR);
		if(fd >= 0) {
			char buf[64];
			int buf_len;
			int err;
			struct dirent *de = (struct dirent *)buf;
	
			vfs_seek(fd, 0, SEEK_SET);
			for(;;) {
				buf_len = sizeof(buf);
				err = vfs_read(fd, buf, -1, &buf_len);
				if(err < 0)
					dprintf(" readdir returned %d\n", err);
				if(buf_len > 0) 
					dprintf(" readdir returned name = '%s'\n", de->name);
				else
					break;
			}
			vfs_close(fd);
		}
		dprintf("devs_init: devfs test done\n");
	
		fd = vfs_open(NULL, "/dev/console", "", STREAM_TYPE_DEVICE);
		dprintf("/dev/console fd = %d\n", fd);
		if(fd >= 0) {
			size_t size = strlen("foo");
			
			vfs_write(fd, "foo", 0, &size);
		}
	}
#endif	

	return 0;
}
