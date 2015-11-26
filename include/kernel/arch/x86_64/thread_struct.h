/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_X86_64_THREAD_STRUCT_H
#define _NEWOS_KERNEL_ARCH_X86_64_THREAD_STRUCT_H

#define IFRAME_TRACE_DEPTH 4

// architecture specific thread info
struct arch_thread {
    addr_t sp;

    // used to track interrupts on this thread
    struct iframe *iframes[IFRAME_TRACE_DEPTH];
    int iframe_ptr;

    // 512 byte floating point save point
    uint8 fpu_state[512];
};

struct arch_proc {
    // nothing here
};

#endif

