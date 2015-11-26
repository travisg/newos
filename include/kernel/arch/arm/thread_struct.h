/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_ARM_THREAD_STRUCT_H
#define _NEWOS_KERNEL_ARCH_ARM_THREAD_STRUCT_H

// architecture specific thread info
struct arch_thread {
    addr_t sp;
};

struct arch_proc {
    // nothing here
};

#endif

