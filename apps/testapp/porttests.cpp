/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscalls.h>
#include <newos/errors.h>


void port_test(void);
static int port_test_thread_func(void* arg);

/*
 * testcode ports
 */

port_id test_p1, test_p2, test_p3, test_p4;

void port_test(void)
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

