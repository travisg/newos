/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <types.h>
#include <libc/string.h>
#include <libc/printf.h>
#include <libsys/syscalls.h>
#include <libsys/stdio.h>

int main()
{
	int fd;
	size_t len;
	char c;
	int rc = 0;

	printf("test\n");

	printf("my thread id is %d\n", sys_get_current_thread_id());
#if 0
	printf("enter something: ");

	for(;;) {
		c = getc();
		printf("%c", c);
	}
#endif
#if 0
	for(;;) {
		sys_snooze(100000);
		printf("booyah!");
	}

	for(;;);
#endif
#if 0
	printf("waiting 5 seconds\n");
	sys_snooze(5000000);
#endif
#if 0
	fd = sys_open("/dev/net/rtl8139/0", "", STREAM_TYPE_DEVICE);
	if(fd >= 0) {
		for(;;) {
			size_t len;
			static char buf[] = {
				0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 
				0x00, 0x08, 0x00, 0x45, 0x00, 0x00, 0x28, 0xF9, 0x55, 0x40, 0x00,
				0x40, 0x06, 0xC0, 0x02, 0xC0, 0xA8, 0x00, 0x01, 0xC0, 0xA8, 0x00, 
				0x26, 0x00, 0x50, 0x0B, 0x5C, 0x81, 0xD6, 0xFA, 0x48, 0xBB, 0x17,
				0x03, 0xC9, 0x50, 0x10, 0x7B, 0xB0, 0x6C, 0x00, 0x00, 0x00, 0x00, 
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
			};
			len = sizeof(buf);
			sys_write(fd, buf, 0, &len);
		}
	}
#endif
#if 1
	fd = sys_open("/dev/net/rtl8139/0", "", STREAM_TYPE_DEVICE);
	if(fd >= 0) {
		int foo = 0;
		for(;;) {
			size_t len;
			char buf[1500];
			
			len = sizeof(buf);
			sys_read(fd, buf, 0, &len);
			printf("%d read %d bytes\n", foo++, len);
		}
	}
#endif

// XXX dangerous! This overwrites the beginning of your hard drive
#if 0
	{
		char buf[512];		
		size_t bytes_read;

		printf("opening /dev/bus/ide/0/0/raw\n");
		fd = sys_open("/dev/bus/ide/0/0/raw", "", STREAM_TYPE_DEVICE);
		printf("fd = %d\n", fd);

		bytes_read = 512;
		rc = sys_read(fd, buf, 0, &bytes_read);
		printf("rc = %d, bytes_read = %d\n", rc, bytes_read);

		buf[0] = 'f';
		buf[1] = 'o';
		buf[2] = 'o';
		buf[3] = '2';
		buf[4] = 0;

		bytes_read = 512;		
		rc = sys_write(fd, buf, 1024, &bytes_read);
		printf("rc = %d, bytes_read = %d\n", rc, bytes_read);

		sys_close(fd);		
	}
#endif

	printf("exiting w/return code %d\n", rc);
	return rc;
}


