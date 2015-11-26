/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _PPC_THREAD_STRUCT_H
#define _PPC_THREAD_STRUCT_H

// architecture specific thread info
struct arch_thread {
    addr_t sp; // current stack pointer
};

struct arch_proc {
    // nothing here
};

#endif

