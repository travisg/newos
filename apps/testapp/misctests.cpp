/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscalls.h>

int syscall_bench(int arg)
{
	bigtime_t start_time = _kern_system_time();
	int i;
	const int count = 250000;

	for(i=0; i < count; i++) {
		_kern_null();
	}

	start_time = _kern_system_time() - start_time;

	printf("took %Ld usecs to call _kern_null() %d times (%Ld usecs/call)\n", 
		start_time, count, start_time/count);
	printf("%Ld cycles/syscall on 500 Mhz cpu\n", start_time * 500 / count);

	return 0;
}
