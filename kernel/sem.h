#ifndef _SEM_H
#define _SEM_H

#include <thread.h>
#include <stage2.h>

struct sem_entry {
	sem_id    id;
	int       count;
	struct thread_queue q;
	thread_id holder_thread;
	char      *name;
};

#define MAX_SEMS 4096

#define SEM_FLAG_NO_RESCHED 1
#define SEM_FLAG_TIMEOUT 2

int sem_init(kernel_args *ka);
sem_id sem_create(int count, char *name);
int sem_delete(sem_id id);
int sem_acquire(sem_id id, int count);
int sem_acquire_etc(sem_id id, int count, int flags, long long timeout);
int sem_release(sem_id id, int count);
int sem_release_etc(sem_id id, int count, int flags);

#endif

