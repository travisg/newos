/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__run_time_linker__hh__
#define __newos__run_time_linker__hh__


#include <types.h>


int rldmain(void *arg);

int load_program(char const *path, void **entry);

void  rldheap_init(void);
void *rldalloc(size_t);
void  rldfree(void *p);


#endif

