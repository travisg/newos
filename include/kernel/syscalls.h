#ifndef _SYSCALLS_H
#define _SYSCALLS_H

int syscall_dispatcher(unsigned long call_num, unsigned long arg0, unsigned long arg1, unsigned long *ret);

#endif

