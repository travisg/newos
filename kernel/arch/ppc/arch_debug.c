/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/arch/debug.h>

// XXX will put stack trace and disassembly routines here

int arch_dbg_init(kernel_args *ka)
{
	return NO_ERROR;
}

int arch_dbg_init2(kernel_args *ka)
{
	return NO_ERROR;
}

void dbg_make_register_file(unsigned int *file, const struct iframe * frame)
{
}
