/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _MALLOC_H
#define _MALLOC_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

void * malloc(size_t);
void free(void *);
void * realloc(void *, size_t);
void * calloc(size_t, size_t);
void * memalign(size_t, size_t);
void * valloc(size_t);

#ifdef __cplusplus
}

void * operator new (size_t);
void * operator new[] (size_t);
void operator delete (void *);
void operator delete[] (void *);

#endif


#endif
