/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__nulibc_unistd__hh__
#define __newos__nulibc_unistd__hh__


#include <types.h>


#ifdef __cplusplus
extern "C" {
#endif


int     open(char const *, int, ...);
int     close(int);
int     dup(int);
int     dup2(int, int);

off_t   lseek(int, off_t, int);
ssize_t read(int, void *, size_t);
ssize_t pread(int, void *, size_t, off_t);
ssize_t write(int, void const*, size_t);
ssize_t pwrite(int, void const*, size_t, off_t);


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif

