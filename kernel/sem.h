#ifndef _SEM_H
#define _SEM_H

#include "thread.h"

struct sem_entry {
	int     sem_id;
	int     count;
	struct thread_queue q;
	int     holder_thread;
	char    *name;
};

#define MAX_SEMS 4096

#define SEM_FLAG_NO_RESCHED 1

int sem_init(struct kernel_args *ka);
int sem_create(int count, char *name);
int sem_acquire(int sem_id, int count);
int sem_acquire_etc(int sem_id, int count, int flags);
int sem_release(int sem_id, int count);
int sem_release_etc(int sem_id, int count, int flags);

#endif

