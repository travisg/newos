/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__libc_string__hh__
#define __newos__libc_string__hh__

#include <stddef.h>
#include <arch/string.h>

#ifdef __cplusplus
namespace std
{extern "C" {
#endif


void *memchr (void const *, int, size_t);
int   memcmp (void const *, const void *, size_t);
void *memcpy (void *, void const *, size_t);
void *memmove(void *, void const *, size_t);
void *memset (void *, int, size_t);

char       *strcat(char *, char const *);
char       *strchr(char const *, int);
int         strcmp(char const *, char const *);
char       *strcpy(char *, char const *);
char const *strerror(int);
size_t      strlen(char const *);
char       *strncat(char *, char const *, size_t);
int         strncmp(char const *, char const *, size_t);
char       *strncpy(char *, char const *, size_t);
char       *strpbrk(char const *, char const *);
char       *strrchr(char const *, int);
size_t      strspn(char const *, char const *);
size_t      strcspn(const char *s, const char *);
char       *strstr(char const *, char const *);
char       *strtok(char *, char const *);
int         strcoll(const char *s1, const char *s2);
size_t      strxfrm(char *dest, const char *src, size_t n);
char       *strdup(const char *str);

#ifdef __cplusplus
}} /* extern "C" */
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* non standard */
void  *bcopy(void const *, void *, size_t);
void   bzero(void *, size_t);
size_t strlcat(char *, char const *, size_t);
size_t strlcpy(char *, char const *, size_t);
int    strncasecmp(char const *, char const *, size_t);
int    strnicmp(char const *, char const *, size_t);
size_t strnlen(char const *s, size_t count);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus) && !defined(_NEWOS_NO_LIBC_COMPAT)

using ::std::memchr;
using ::std::memcmp;
using ::std::memcpy;
using ::std::memmove;
using ::std::memset;

using ::std::memchr;
using ::std::memcmp;
using ::std::memcpy;
using ::std::memmove;
using ::std::memset;
using ::std::strcat;
using ::std::strchr;
using ::std::strcmp;
using ::std::strcoll;
using ::std::strcpy;
using ::std::strspn;
using ::std::strcspn;
using ::std::strerror;
//using ::std::atrerror;
using ::std::strlen;
using ::std::strncat;
using ::std::strncmp;
using ::std::strncpy;
using ::std::strpbrk;
using ::std::strrchr;
//using ::std::strapn;
using ::std::strstr;
using ::std::strtok;
using ::std::strxfrm;
using ::std::strdup;

#endif

#endif
