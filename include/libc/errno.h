/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef _newos__errno__hh_
#define _newos__errno__hh_


#ifdef __cplusplus
extern "C"
{
#endif


/*
 * this will change when we get TLS working
 */
extern int errno;



/*
 * These need to match system errors... will do some other day,
 * right now i'm just happy getting stdio to compile
 */
#define ENOMEM	0xF0000000
#define EBADF	0xF0000001
#define EINVAL	0xF0000002


#ifdef __cplusplus
} /* "C" */
#endif


#endif
