/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__nulibc_stdlib__hh__
#define __newos__nulibc_stdlib__hh__


#include <types.h>


#ifdef __cplusplus
extern "C" {
#endif


int	      atoi(char const *); 
unsigned int  atoui(const char *num);
long          atol(const char *num);
unsigned long atoul(const char *num);

void * malloc(size_t);
void   free(void *);
void * realloc(void *, size_t);
void * calloc(size_t, size_t);
void * memalign(size_t, size_t);
void * valloc(size_t);

char *getenv(char const *);
int   setenv(char const *, char const *, int);
int   putenv(char const *);
void  unsetenv(char const *);


#ifdef __cplusplus
} /* extern "C" */
#endif


#ifdef __cplusplus
void * operator new (size_t);
void * operator new[] (size_t);
void operator delete (void *);
void operator delete[] (void *);
#endif


#endif
