/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#define SYSCALL0(name, n) \
.globl name; \
.align 4; \
name:

#define SYSCALL1(name, n) \
.globl name; \
.align 4; \
name:

#define SYSCALL2(name, n) \
.globl name; \
.align 4; \
name:

#define SYSCALL3(name, n) \
.globl name; \
.align 4; \
name:

#define SYSCALL4(name, n) \
.globl name; \
.align 4; \
name:

#define SYSCALL5(name, n) \
.globl name; \
.align 4; \
name:

#define SYSCALL6(name, n) \
.globl name; \
.align 4; \
name:

SYSCALL0(sys_null, 0)
SYSCALL3(sys_open, 1)
SYSCALL4(sys_seek, 2)
SYSCALL5(sys_read, 3)
SYSCALL5(sys_write, 4)
SYSCALL4(sys_ioctl, 5)
SYSCALL1(sys_close, 6)
SYSCALL3(sys_create, 7)
SYSCALL0(sys_system_time, 8)
SYSCALL2(sys_snooze, 9)
SYSCALL2(sys_sem_create, 10)
SYSCALL1(sys_sem_delete, 11)
SYSCALL2(sys_sem_acquire, 12)
SYSCALL5(sys_sem_acquire_etc, 13)
SYSCALL2(sys_sem_release, 14)
SYSCALL3(sys_sem_release_etc, 15)
SYSCALL0(sys_get_current_thread_id, 16)
SYSCALL1(sys_exit, 17)
SYSCALL3(sys_proc_create_proc, 18)
SYSCALL2(sys_thread_wait_on_thread, 19)
SYSCALL2(sys_proc_wait_on_proc, 20)
SYSCALL6(sys_vm_create_anonymous_region, 21)
SYSCALL5(sys_vm_clone_region, 22);
//SYSCALL0(sys_vm_mmap_file, 23);
SYSCALL1(sys_vm_delete_region, 24);
SYSCALL1(sys_vm_find_region_by_name, 25);
SYSCALL2(sys_vm_get_region_info, 26);

