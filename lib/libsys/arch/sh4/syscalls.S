/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

/*
** arg layout:
** r4-r7:  arg 1 - 4
** r0-r3:  arg 5 - 8
** r8-r13: arg 8 - 13
*/
#define FUNC(name) .align 2 ; .globl _##name ; _##name:

#define SYSCALL0(name, n) \
FUNC(name) \
	trapa	n; \
	nop; \
	rts; \
	nop;

#define SYSCALL1(name, n) \
FUNC(name) \
	trapa	n; \
	rts; \
	nop;

#define SYSCALL2(name, n) \
FUNC(name) \
	trapa	n; \
	nop; \
	rts; \
	nop;

#define SYSCALL3(name, n) \
FUNC(name) \
	trapa	n; \
	nop; \
	rts; \
	nop;

#define SYSCALL4(name, n) \
FUNC(name) \
	trapa	n; \
	nop; \
	rts; \
	nop; \

#define SYSCALL5(name, n) \
FUNC(name) \
	mov.l	@r15,r0; \
	trapa	n; \
	nop; \
	rts; \
	nop;

#define SYSCALL6(name, n) \
FUNC(name) \
	mov.l	@(0,r15),r0; \
	mov.l	@(4,r15),r1; \
	trapa	n; \
	nop; \
	rts; \
	nop;
#define SYSCALL7(name, n) \
FUNC(name) \
	mov.l	@(0,r15),r0; \
	mov.l	@(4,r15),r1; \
	mov.l	@(8,r15),r2; \
	trapa	n; \
	nop; \
	rts; \
	nop;

#define SYSCALL8(name, n) \
FUNC(name) \
	mov.l	@(0,r15),r0; \
	mov.l	@(4,r15),r1; \
	mov.l	@(8,r15),r2; \
	mov.l	@(12,r15),r3; \
	trapa	n; \
	nop; \
	rts; \
	nop;

#define SYSCALL9(name, n) \
FUNC(name) \
	mov.l	@(0,r15),r0; \
	mov.l	@(4,r15),r1; \
	mov.l	@(8,r15),r2; \
	mov.l	@(12,r15),r3; \
	mov.l	r8,@-r15; \
	mov.l	@(20,r15),r8; \
	trapa	n; \
	nop; \
	rts; \
	mov.l	@r15+,r8

SYSCALL0(sys_null, #0)
SYSCALL4(sys_mount, #1)
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


