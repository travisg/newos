#ifndef _PROC_H
#define _PROC_H

#include "stage2.h"
#include "vm.h"
#include "thread.h"

int proc_init(struct kernel_args *ka);
struct proc *proc_get_kernel_proc();

struct proc {
	struct proc *next;
	char *name;
	int id;
	struct aspace *aspace;
	struct thread *thread_list;
};

struct proc_key {
	int id;
};


#endif

