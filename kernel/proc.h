#ifndef _PROC_H
#define _PROC_H

#include "stage2.h"
#include "vm.h"
#include "thread.h"

struct proc {
	struct proc *next;
	proc_id id;
	char *name;
	struct aspace *aspace;
	struct thread *thread_list;
};

struct proc_key {
	proc_id id;
};

int proc_init(kernel_args *ka);
struct proc *proc_get_kernel_proc();

#endif

