/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ELF_H
#define _ELF_H

#include <kernel/thread.h>

int elf_load_uspace(const char *path, struct proc *p, int flags, addr *entry);
int elf_init(kernel_args *ka);

#endif

