/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__libc_stdlib__hh__
#define __newos__libc_stdlib__hh__


#include <newos/types.h>


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
void * reallocf(void *, size_t);
void * calloc(size_t, size_t);
void * memalign(size_t, size_t);
void * valloc(size_t);

/* terrible hack to get around the different kernel name for malloc and free */
#if KERNEL
#define malloc kmalloc
#define free kfree
#include <kernel/heap.h>
#endif

char *getenv(char const *);
int   setenv(char const *, char const *, int);
int   putenv(char const *);
void  unsetenv(char const *);

void *bsearch(void const *, void const *, size_t, size_t, int (*) (void const *, void const *));
int   heapsort(void *, size_t , size_t , int (*)(void const *, void const *));
int   mergesort(void *, size_t, size_t, int (*)(void const *, void const *));
void  qsort(void *, size_t, size_t, int (*)(void const *, void const *));
int   radixsort(u_char const **, int, u_char const *, u_int);
int   sradixsort(u_char const **, int, u_char const *, u_int);


#define	RAND_MAX	0x7fffffff
int   rand(void);
int   rand_r(unsigned int *ctx);
void  srand(unsigned);
long  random(void);
void  srandom(unsigned long);

#if !KERNEL
void abort(void);
void exit(int);
void _exit(int);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif


#ifdef __cplusplus
void * operator new (size_t);
void * operator new[] (size_t);
void operator delete (void *);
void operator delete[] (void *);
#endif

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#endif
