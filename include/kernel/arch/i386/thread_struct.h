/* 
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_I386_THREAD_STRUCT_H
#define _NEWOS_KERNEL_ARCH_I386_THREAD_STRUCT_H

// architecture specific thread info
struct arch_thread {
	unsigned int *esp;
};

struct arch_proc {
	// nothing here
};

#endif

