/*
** Copyright 2004, Michael Noisternig. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _X86_64_SELECTOR_H_
#define _X86_64_SELECTOR_H_

#include <arch/x86_64/types.h>

typedef uint32 selector_id;
typedef uint64 selector_type;

// DATA segments are read-only
// CODE segments are execute-only
// both can be modified by using the suffixed enum versions
// legend:	w = writable
//			d = expand down
//			r = readable
//			c = conforming

#endif

