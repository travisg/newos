/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _SH4_THREAD_STRUCT_H
#define _SH4_THREAD_STRUCT_H

// architecture specific thread info
struct arch_thread {
	unsigned int *sp;
};

struct arch_proc {
};

#endif

