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
SYSCALL4(sys_stat, 8)
SYSCALL0(sys_system_time, 9)
SYSCALL2(sys_snooze, 10)
SYSCALL2(sys_sem_create, 11)
SYSCALL1(sys_sem_delete, 12)
SYSCALL2(sys_sem_acquire, 13)
SYSCALL5(sys_sem_acquire_etc, 14)
SYSCALL2(sys_sem_release, 15)
SYSCALL3(sys_sem_release_etc, 16)
SYSCALL0(sys_get_current_thread_id, 17)
SYSCALL1(sys_exit, 18)
SYSCALL3(sys_proc_create_proc, 19)
SYSCALL2(sys_thread_wait_on_thread, 20)
SYSCALL2(sys_proc_wait_on_proc, 21)
SYSCALL6(sys_vm_create_anonymous_region, 22)
SYSCALL5(sys_vm_clone_region, 23);
//SYSCALL0(sys_vm_mmap_file, 24);
SYSCALL1(sys_vm_delete_region, 25);
SYSCALL1(sys_vm_find_region_by_name, 26);
SYSCALL2(sys_vm_get_region_info, 27);
SYSCALL3(sys_thread_create_thread, 28);
SYSCALL1(sys_thread_kill_thread, 29);
SYSCALL1(sys_thread_suspend_thread, 30);
SYSCALL1(sys_thread_resume_thread, 31);
SYSCALL1(sys_proc_kill_proc, 32);
SYSCALL0(sys_get_current_proc_id, 33);

