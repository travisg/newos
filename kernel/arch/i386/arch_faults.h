#ifndef _ARCH_FAULTS
#define _ARCH_FAULTS

int arch_faults_init(struct kernel_args *ka);

void i386_general_protection_fault(int errorcode);
void i386_double_fault(int errorcode);

#endif

