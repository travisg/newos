/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <string.h>
#include <libsys/syscalls.h>
#include <kernel/user_runtime.h>
#include "rld_priv.h"



int
rldmain(void *arg)
{
	int retval;
	unsigned long entry;
	struct uspace_prog_args_t *rld_args= (struct uspace_prog_args_t *)arg;

	rldheap_init();

	entry= 0;

	load_program(rld_args->prog_path, (void**)&entry);

	if(entry) {
		retval= ((int(*)(void*))(entry))(rld_args);
	} else {
		return -1;
	}

	return retval;
}

