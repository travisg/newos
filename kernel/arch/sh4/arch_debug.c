/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/arch/debug.h>

int arch_dbg_init(kernel_args *ka)
{
    return 0;
}

int arch_dbg_init2(kernel_args *ka)
{
    return 0;
}

void dbg_make_register_file(unsigned int *file, const struct iframe *frame)
{
    // XXX gdb stub for SH4 obviously not implemented.
}
