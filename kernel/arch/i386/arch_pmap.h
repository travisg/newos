#ifndef _I386_PMAP_H
#define _I386_PMAP_H

#include "stage2.h"

int arch_pmap_init(struct kernel_args *ka);
int arch_pmap_init2(struct kernel_args *ka);
int pmap_map_page(unsigned int paddr, unsigned int vaddr);
int pmap_unmap_page(unsigned int vaddr);
void arch_pmap_invl_page(unsigned int vaddr);

#endif

