/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
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
	const int count = 50000;

	for(i=0; i < count; i++) {
		_kern_null();
	}

	start_time = _kern_system_time() - start_time;

	printf("took %Ld usecs to call _kern_null() %d times (%Ld usecs/call)\n", 
		start_time, count, start_time/count);

	return 0;
}
