#ifndef _SPINLOCK_H
#define _SPINLOCK_H

void acquire_spinlock(int *lock);
void release_spinlock(int *lock);

#endif

