#ifndef _FAULTS_PRIV_H
#define _FAULTS_PRIV_H

int general_protection_fault(int errorcode);

enum fpu_faults {
	FPU_FAULT_CODE_UNKNOWN = 0,
	FPU_FAULT_CODE_DIVBYZERO,
	FPU_FAULT_CODE_INVALID_OP,
	FPU_FAULT_CODE_OVERFLOW,
	FPU_FAULT_CODE_UNDERFLOW,
	FPU_FAULT_CODE_INEXACT
};
int fpu_fault(int fpu_fault);

int fpu_disable_fault();

#endif

