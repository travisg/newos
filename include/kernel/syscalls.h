#ifndef _SYSCALLS_H
#define _SYSCALLS_H

enum {
	SYSCALL_NULL = 0,
	SYSCALL_OPEN,
	SYSCALL_SEEK,
	SYSCALL_READ,
	SYSCALL_WRITE,
	SYSCALL_IOCTL,
	SYSCALL_CLOSE,
	SYSCALL_CREATE,
	SYSCALL_SYSTEM_TIME,
	SYSCALL_SNOOZE,
};

int syscall_dispatcher(unsigned long call_num, unsigned long arg0, unsigned long arg1,
	unsigned long arg2, unsigned long arg3, unsigned long arg4, uint64 *call_ret);

#endif

