#ifndef _ARCH_VM_H
#define _ARCH_VM_H

#include <stage2.h>

int arch_vm_init(kernel_args *ka);
int arch_vm_init2(kernel_args *ka);
int map_page_into_kspace(addr paddr, addr kaddr);
int i386_page_fault(int cr2reg, unsigned int fault_address);

#endif

