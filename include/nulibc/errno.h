#ifndef __newos__errno__hh__
#define __newos__errno_hh__


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
