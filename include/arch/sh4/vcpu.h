#ifndef _VCPU_H
#define _VCPU_H

#include <boot/stage2.h>
#include <arch/sh4/vcpu_struct.h>

// page table structures
struct pdent {
	unsigned int v:1;
	unsigned int unused0:11;
	unsigned int ppn:17;
	unsigned int unused1:3;
};

struct ptent {
	unsigned int v:1;
	unsigned int d:1;
	unsigned int sh:1;
	unsigned int sz:2;
	unsigned int c:1;
	unsigned int tlb_ent:6;
	unsigned int ppn:17;
	unsigned int pr:2;
	unsigned int wt:1;
};

// soft faults
#define EXCEPTION_PAGE_FAULT 0xff

// can only be used in stage2
int vcpu_init(kernel_args *ka);

#endif

