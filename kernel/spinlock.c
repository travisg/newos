#include "arch_cpu.h"
#include "spinlock.h"

void acquire_spinlock(int *lock)
{
	while(1) {
		while(*lock)
			;
		if(!test_and_set(lock, 1))
			break;
	}
}

void release_spinlock(int *lock)
{
	*lock = 0;
}

