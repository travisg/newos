#ifndef _ARCH_FAULTS
#define _ARCH_FAULTS

int arch_faults_init(struct kernel_args *ka);

int i386_general_protection_fault(int errorcode);
int i386_double_fault(int errorcode);

#endif

