/*
** Copyright 2003 Graham Batty. All rights reserved.
** Distributed under the terms of the NewOS License
*/

#ifndef __newos__libc_stddef_hh__
# define __newos__libc_stddef_hh__

# ifdef __cplusplus
namespace std
{extern "C" {
# endif

# define NULL 0
// XX note that this is GCC toolchain specific
# define offsetof(type, member) __offsetof(type, member)

typedef ::_newos_size_t size_t;
typedef ::off_t ptrdiff_t;

# ifdef __cplusplus
}}
# endif

#endif

// note that we want the (harmless) using declarations whether
// we're double included or not in case someone #includes <cstddef>
// and then <stddef.h>
#if defined(__cplusplus) && !defined(_NEWOS_NO_LIBC_COMPAT)

using ::std::size_t;
using ::std::ptrdiff_t;

#endif
