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
#if 1
	printf("waiting 5 seconds\n");
	sys_snooze(5000000);
#endif
#if 0
	fd = sys_open("/dev/bus/pci/0/15/0/ctrl", "", STREAM_TYPE_DEVICE);
	if(fd >= 0) {
		sys_ioctl(fd, 10100, NULL, 0);
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


