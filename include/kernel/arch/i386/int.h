/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_I386_INT_H
#define _NEWOS_KERNEL_ARCH_I386_INT_H

/* x86 supports 256 interrupt vectors,
 * but all io interrupts are mapped to 0x20+, so we
 * only need to have a table big enough to hold 256 - 0x20 vectors
 */
#define ARCH_NUM_INT_VECTORS (256-0x20)

#endif

