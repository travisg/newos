#ifndef _MMU_H
#define _MMU_H

#include <stage2.h>

void mmu_init(kernel_args *ka, unsigned int *next_paddr);
void mmu_map_page(unsigned int vaddr, unsigned int paddr);

#endif

