#include "arch_cpu.h"
#include "spinlock.h"

void acquire_spinlock(int *lock)
{
	while(1) {
		while(*lock != 0)
			;
		if(test_and_set(lock, 1) == 0)
			break;
	}
}

void release_spinlock(int *lock)
{
	*lock = 0;
}

