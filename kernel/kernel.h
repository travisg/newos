#ifndef _KERNEL_H
#define _KERNEL_H

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))

#define KSTACK_SIZE (PAGE_SIZE*2)

#define MAX_CPUS 2

#endif

