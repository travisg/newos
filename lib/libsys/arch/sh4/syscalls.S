/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define FUNC(name) .align 2 ; .globl _##name ; _##name:

#define SYSCALL0(name, n) \
FUNC(name) \
	trapa	n
#define SYSCALL1(name, n) \
FUNC(name) \
	trapa	n
#define SYSCALL2(name, n) \
FUNC(name) \
	trapa	n
#define SYSCALL3(name, n) \
FUNC(name) \
	trapa	n
#define SYSCALL4(name, n) \
FUNC(name) \
	trapa	n
#define SYSCALL5(name, n) \
FUNC(name) \
	mov.l	@r15,r0; \
	trapa	n;
#define SYSCALL6(name, n) \
FUNC(name) \
	mov		r15,r3; \
	mov.l	@r3+,r0; \
	mov.l	@r3+,r1; \
	trapa	n;
#define SYSCALL7(name, n) \
FUNC(name) \
	mov		r15,r3; \
	mov.l	@r3+,r0; \
	mov.l	@r3+,r1; \
	mov.l	@r3+,r2; \
	trapa	n;
#define SYSCALL8(name, n) \
FUNC(name) \
	mov		r15,r3; \
	mov.l	@r3+,r0; \
	mov.l	@r3+,r1; \
	mov.l	@r3+,r2; \
	mov.l	@r3+,r3; \
	trapa	n;
#define SYSCALL9(name, n) \
FUNC(name) \
	mov.l	r8,@-r15; \
	mov		r15,r8; \
	add		#4,r8; \
	mov.l	@r8+,r0; \
	mov.l	@r8+,r1; \
	mov.l	@r8+,r2; \
	mov.l	@r8+,r3; \
	mov.l	@r8+,r8; \
	trapa	n;

SYSCALL0(sys_null, #0)
SYSCALL2(sys_mount, #1)
SYSCALL1(sys_unmount, #2)
SYSCALL0(sys_sync, #3)
SYSCALL3(sys_open, #4)
SYSCALL1(sys_close, #5)
SYSCALL1(sys_fsync, #6)
SYSCALL5(sys_read, #7)
SYSCALL5(sys_write, #8)
SYSCALL4(sys_seek, #9)
SYSCALL4(sys_ioctl, #10)
SYSCALL2(sys_create, #11)
SYSCALL1(sys_unlink, #12)
SYSCALL2(sys_rename, #13)
SYSCALL2(sys_rstat, #14)
SYSCALL3(sys_wstat, #15)
SYSCALL0(sys_system_time, #16)
SYSCALL2(sys_snooze, #17)
SYSCALL2(sys_sem_create, #18)
SYSCALL1(sys_sem_delete, #19)
SYSCALL2(sys_sem_acquire, #20)
SYSCALL5(sys_sem_acquire_etc, #21)
SYSCALL2(sys_sem_release, #22)
SYSCALL3(sys_sem_release_etc, #23)
SYSCALL0(sys_get_current_thread_id, #24)
SYSCALL1(sys_exit, #25)
SYSCALL3(sys_proc_create_proc, #26)
SYSCALL2(sys_thread_wait_on_thread, #27)
SYSCALL2(sys_proc_wait_on_proc, #28)
SYSCALL6(sys_vm_create_anonymous_region, #29)
SYSCALL6(sys_vm_clone_region, #30)
SYSCALL9(sys_vm_map_file, #31)
SYSCALL1(sys_vm_delete_region, #32)
SYSCALL2(sys_vm_get_region_info, #33)
SYSCALL3(sys_thread_create_thread, #34)
SYSCALL1(sys_thread_kill_thread, #35)
SYSCALL1(sys_thread_suspend_thread, #36)
SYSCALL1(sys_thread_resume_thread, #37)
SYSCALL1(sys_proc_kill_proc, #38)
SYSCALL0(sys_get_current_proc_id, #39)
SYSCALL2(sys_getcwd, #40)
SYSCALL1(sys_setcwd, #41)


