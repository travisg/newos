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
	printf("exiting\n");
	return 0;
}


