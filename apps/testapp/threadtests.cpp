/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscalls.h>

int sleep_test(int arg)
{
	printf("should display 'booyah!' 10 times, one second apart\n");
	for(int i = 0; i < 10; i++) {
		_kern_snooze(1000000);
		printf("booyah!");
	}

	return 0;
}

static int test_thread(void *args)
{
	int i = (int)args;

	for(;;) {
		printf("%c", 'a' + i);
	}
	return 0;
}

static int test_thread_self_terminate(void *args)
{
	int i = (int)args;
	bigtime_t start_time = _kern_system_time();

	for(;;) {
		if(_kern_system_time() - start_time >= 10000000) {
			printf("thread %c terminating...\n", 'a' + i);
			break;
		}
		printf("%c", 'a' + i);
	}
	return 0;
}

#if 0
static int dummy_thread(void *args)
{
	return 1;
}

static int cpu_eater_thread(void *args)
{
	for(;;)
		;
}

static int fpu_cruncher_thread(void *args)
{
	double y = *(double *)args;
	double z;

	for(;;) {
		z = y * 1.47;
		y = z / 1.47;
		if(y != *(double *)args)
			printf("error: y %f\n", y);
	}
}
#endif

int thread_spawn_test(int arg)
{
	int i;
	int (*thread_func)(void *) = NULL;
	int num_threads = 10;

	if(arg == 0) {
		printf("creating 10 threads, runs forever...\n");
		thread_func = &test_thread;
	} else if(arg == 1) {
		printf("creating 10 threads, then killing them after 10 seconds\n");
		thread_func = &test_thread;
	} else if(arg == 2) {
		printf("creating 10 threads, self terminating after 10 seconds\n");
		thread_func = &test_thread_self_terminate;
	} else
		return -1;

	thread_id tids[num_threads];
	for(i=0; i<num_threads; i++) {
		tids[i] = _kern_thread_create_thread("foo", thread_func, (void *)i);
		_kern_thread_resume_thread(tids[i]);
	}

	if(arg == 0) {
		for(;;)
			_kern_snooze(1000000);
	} else if(arg == 1) {
		_kern_snooze(10000000);
		for(i=0; i<num_threads; i++) {
			printf("killing thread %d...\n", tids[i]);
			_kern_thread_kill_thread(tids[i]);
		}
	} else if(arg == 2) {
		_kern_thread_wait_on_thread(tids[0], NULL);
		_kern_snooze(1000000);
	}

	printf("done\n");
	return 0;
}

