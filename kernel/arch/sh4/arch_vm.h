#ifndef _ARCH_VM_H
#define _ARCH_VM_H

#include <stage2.h>

int arch_vm_init(struct kernel_args *ka);
int arch_vm_init2(struct kernel_args *ka);
int map_page_into_kspace(unsigned int paddr, unsigned int kaddr);

#endif

