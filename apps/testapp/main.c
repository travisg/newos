/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <types.h>
#include <libc/string.h>
#include <libc/printf.h>
#include <libsys/syscalls.h>
#include <libsys/stdio.h>

int test_thread(void *args)
{
	int i = (int)args;

	sys_snooze(1000000);
	for(;;) {
		printf("%c", 'a' + i);
	}
	return 0;
}

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
#if 0
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
#if 1
	{
		thread_id tids[10];
		int i;

		for(i=0; i<10; i++) {
			tids[i] = sys_thread_create_thread("foo", &test_thread, (void *)i);
			sys_thread_resume_thread(tids[i]);
		}

		sys_snooze(5000000);
		sys_proc_kill_proc(sys_get_current_proc_id());
/*
		sys_snooze(3000000);
		for(i=0; i<10; i++) {
			sys_thread_kill_thread(tids[i]);
		}
		printf("thread_is dead\n");
		sys_snooze(5000000);
*/
	}
#endif
#if 0
	{
		for(;;)
			sys_proc_create_proc("/boot/bin/true", "true", 32);
	}
#endif
#if 0
	{
		void *buf = (void *)0x60000000;
		int fd;
		int rc;
		int len = 512;

		fd = sys_open("/boot/testapp", "", STREAM_TYPE_FILE);

		rc = sys_read(fd, buf, 0, &len);
		printf("rc from read = 0x%x\n", rc);
		sys_close(fd);
	}
#endif
#if 0
	{
		char data;
		int fd;

		fd = sys_open("/dev/audio/pcbeep/1", STREAM_TYPE_DEVICE, 0);
		if(fd >= 0) {
			printf("writing to the speaker\n");
			data = 3;
			sys_write(fd, &data, 0, 1);
			sys_snooze(1000000);
			data = 0;
			sys_write(fd, &data, 0, 1);
			sys_close(fd);
		}
	}
#endif
	printf("exiting w/return code %d\n", rc);
	return rc;
}


