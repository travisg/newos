#ifndef _ELF_H
#define _ELF_H

#include <thread.h>

int elf_load(const char *path, struct proc *p, int flags, addr *entry);

#endif

