/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__libc_fcntl__hh__
#define __newos__libc_fcntl__hh__

#define O_RDONLY	0x0
#define O_WRONLY	0x1
#define O_RDWR		0x2
#define O_RWMASK	0x3
#define O_ACCMODE	O_RWMASK

#define O_CREAT		0x0010		/* create if it doesn't already exist */
#define O_EXCL		0x0020		/* error if it already does */
#define O_TRUNC		0x0040		/* truncate to zero length */
#define O_APPEND	0x0080		/* append to the end of the file */
#define O_CLOEXEC	0x0100		/* close-on-exec */

#endif

