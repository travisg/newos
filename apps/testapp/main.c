/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <types.h>
#include <libc/string.h>
#include <libc/printf.h>
#include <libsys/syscalls.h>
#include <libsys/stdio.h>
#include <sys/errors.h>

static void port_test(void);
static int port_test_thread_func(void* arg);

static int test_thread(void *args)
{
	int i = (int)args;

	sys_snooze(1000000);
	for(;;) {
		printf("%c", 'a' + i);
	}
	return 0;
}

int main(int argc, char **argv)
{
	int fd;
	size_t len;
	char c;
	int rc = 0;
	int cnt;

	printf("testapp\n");

	for(cnt = 0; cnt< argc; cnt++) {
		printf("arg %d = %s \n",cnt,argv[cnt]);
	}

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
#if 0
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

#if 0
	{
		port_test();
	}
#endif

	printf("exiting w/return code %d\n", rc);
	return rc;
}

/*
 * testcode ports
 */

port_id test_p1, test_p2, test_p3, test_p4;

static void port_test(void)
{
	char testdata[5];
	thread_id t;
	int res;
	int32 dummy;
	int32 dummy2;

	strcpy(testdata, "abcd");

	printf("porttest: port_create()\n");
	test_p1 = sys_port_create(1,    "test port #1");
	test_p2 = sys_port_create(10,   "test port #2");
	test_p3 = sys_port_create(1024, "test port #3");
	test_p4 = sys_port_create(1024, "test port #4");

	printf("porttest: port_find()\n");
	printf("'test port #1' has id %d (should be %d)\n", sys_port_find("test port #1"), test_p1);

	printf("porttest: port_write() on 1, 2 and 3\n");
	sys_port_write(test_p1, 1, &testdata, sizeof(testdata));
	sys_port_write(test_p2, 666, &testdata, sizeof(testdata));
	sys_port_write(test_p3, 999, &testdata, sizeof(testdata));
	printf("porttest: port_count(test_p1) = %d\n", sys_port_count(test_p1));

	printf("porttest: port_write() on 1 with timeout of 1 sec (blocks 1 sec)\n");
	sys_port_write_etc(test_p1, 1, &testdata, sizeof(testdata), PORT_FLAG_TIMEOUT, 1000000);
	printf("porttest: port_write() on 2 with timeout of 1 sec (wont block)\n");
	res = sys_port_write_etc(test_p2, 777, &testdata, sizeof(testdata), PORT_FLAG_TIMEOUT, 1000000);
	printf("porttest: res=%d, %s\n", res, res == 0 ? "ok" : "BAD");

	printf("porttest: port_read() on empty port 4 with timeout of 1 sec (blocks 1 sec)\n");
	res = sys_port_read_etc(test_p4, &dummy, &dummy2, sizeof(dummy2), PORT_FLAG_TIMEOUT, 1000000);
	printf("porttest: res=%d, %s\n", res, res == ERR_PORT_TIMED_OUT ? "ok" : "BAD");

	printf("porttest: spawning thread for port 1\n");
	t = sys_thread_create_thread("port_test", port_test_thread_func, NULL);
	// resume thread
	sys_thread_resume_thread(t);
	
	printf("porttest: write\n");
	sys_port_write(test_p1, 1, &testdata, sizeof(testdata));

	// now we can write more (no blocking)
	printf("porttest: write #2\n");
	sys_port_write(test_p1, 2, &testdata, sizeof(testdata));
	printf("porttest: write #3\n");
	sys_port_write(test_p1, 3, &testdata, sizeof(testdata));

	printf("porttest: waiting on spawned thread\n");
	sys_thread_wait_on_thread(t, NULL);

	printf("porttest: close p1\n");
	sys_port_close(test_p2);
	printf("porttest: attempt write p1 after close\n");
	res = sys_port_write(test_p2, 4, &testdata, sizeof(testdata));
	printf("porttest: port_write ret %d\n", res);

	printf("porttest: testing delete p2\n");
	sys_port_delete(test_p2);

	printf("porttest: end test main thread\n");
}

static int port_test_thread_func(void* arg)
{
	int msg_code;
	int n;
	char buf[5];

	printf("porttest: port_test_thread_func()\n");
	
	n = sys_port_read(test_p1, &msg_code, &buf, 3);
	printf("port_read #1 code %d len %d buf %3s\n", msg_code, n, buf);
	n = sys_port_read(test_p1, &msg_code, &buf, 4);
	printf("port_read #1 code %d len %d buf %4s\n", msg_code, n, buf);
	buf[4] = 'X';
	n = sys_port_read(test_p1, &msg_code, &buf, 5);
	printf("port_read #1 code %d len %d buf %5s\n", msg_code, n, buf);

	printf("porttest: testing delete p1 from other thread\n");
	sys_port_delete(test_p1);
	printf("porttest: end port_test_thread_func()\n");
	
	return 0;
}

