#ifndef _VCPU_STRUCT_H
#define _VCPU_STRUCT_H

struct vector {
	int (*func)(unsigned int exception_code, unsigned int pc, unsigned int trap_code, unsigned int page_fault_addr);
};

typedef struct {
	unsigned int *kernel_pgdir;
	unsigned int *user_pgdir;
	unsigned int kernel_asid;
	unsigned int user_asid;
	unsigned int *kstack;
	int handling_exception;
	struct vector vt[256];
} vcpu_struct;

#endif

