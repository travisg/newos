/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef _newos__errno__hh_
#define _newos__errno__hh_

#include <newos/errors.h>

#ifdef __cplusplus
namespace std
{extern "C" {
#endif


/*
 * this will change when we get TLS working
 */
#if KERNEL
#define __WITH_ERRNO 0
#else
#define __WITH_ERRNO 1
#endif

#if __WITH_ERRNO
extern int errno;
#endif

/* mapping posix errors to system errors */
#define EPERM   ERR_PERMISSION_DENIED
#define ENOENT  ERR_NOT_FOUND
//#define ESRCH
//#define EINTR
#define EIO     ERR_IO_ERROR
//#define ENXIO
#define E2BIG   ERR_TOO_BIG
//#define ENOEXEC
#define EBADF   ERR_INVALID_HANDLE
//#define ECHILD
//#define EDEADLK
#define ENOMEM  ERR_NO_MEMORY
#define EACCES  ERR_PERMISSION_DENIED
//#define EFAULT
//#define EBUSY
#define EEXIST  ERR_VFS_ALREADY_EXISTS
//#define EXDEV
#define ENODEV  ERR_VFS_WRONG_STREAM_TYPE
#define ENOTDIR ERR_VFS_NOT_DIR
//#define EISDIR
#define EINVAL  ERR_INVALID_ARGS
#define ENFILE  ERR_NO_MORE_HANDLES
#define EMFILE  ERR_NO_MORE_HANDLES
//#define ENOTTY
//#define EFBIG
//#define ENOSPC
#define EROFS   ERR_VFS_READONLY_FS
//#define EMLINK
#define EPIPE   ERR_PIPE_WIDOW

//#define EDOM
#define ERANGE  ERR_OUT_OF_RANGE

//#define EAGAIN
//#define EWOULDBLOCK
//#define EINPROGRESS
//#define EALREADY

/* network stuff */
//#define ENOTSOCK
//#define EDESTADDRREQ
//#define EMSGSIZE
//#define EPROTOTYPE
//#define ENOPROTOOPT
//#define EPROTONOSUPPORT
//#define ESOCKTNOSUPPORT
//#define EOPNOTSUPP
//#define ENOTSUP
//#define EPFNOSUPPORT
//#define EAFNOSUPPORT
//#define EADDRINUSE
//#define EADDRNOTAVAIL


#ifdef __cplusplus
}} /* "C" */
#endif


#endif

#if defined(__cplusplus) && !defined(_NEWOS_NO_LIBC_COMPAT)
using ::std::errno;
#endif
