/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/arch/debug.h>

void dbg_make_register_file(unsigned int *file, const struct iframe *frame)
{
	// XXX gdb stub for SH4 obviously not implemented.
}
