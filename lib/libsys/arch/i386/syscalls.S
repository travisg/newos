/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

/*
** syscall interface works as such:
** eax has syscall #
** ecx has number of args (0-16)
** edx has pointer to buffer containing args from first to last
** each is verified to make sure someone doesnt try to clobber it
*/

#define SYSCALL0(name, n) \
.globl name; \
.align 8; \
name: \
	movl	$n,%eax; \
	jmp		syscall0

#define SYSCALL1(name, n) \
.globl name; \
.align 8; \
name: \
	movl	$n,%eax; \
	jmp		syscall1

#define SYSCALL2(name, n) \
.globl name; \
.align 8; \
name: \
	movl	$n,%eax; \
	jmp		syscall2

#define SYSCALL3(name, n) \
.globl name; \
.align 8; \
name: \
	movl	$n,%eax; \
	jmp		syscall3

#define SYSCALL4(name, n) \
.globl name; \
.align 8; \
name: \
	movl	$n,%eax; \
	jmp		syscall4

#define SYSCALL5(name, n) \
.globl name; \
.align 8; \
name: \
	movl	$n,%eax; \
	jmp		syscall5

#define SYSCALL6(name, n) \
.globl name; \
.align 8; \
name: \
	movl	$n,%eax; \
	jmp		syscall6

SYSCALL0(sys_null, 0)
SYSCALL2(sys_mount, 1)
SYSCALL1(sys_unmount, 2)
SYSCALL0(sys_sync, 3)
SYSCALL3(sys_open, 4)
SYSCALL1(sys_close, 5)
SYSCALL1(sys_fsync, 6)
SYSCALL5(sys_read, 7)
SYSCALL5(sys_write, 8)
SYSCALL4(sys_seek, 9)
SYSCALL4(sys_ioctl, 10)
SYSCALL2(sys_create, 11)
SYSCALL1(sys_unlink, 12)
SYSCALL2(sys_rename, 13)
SYSCALL2(sys_rstat, 14)
SYSCALL3(sys_wstat, 15)
SYSCALL0(sys_system_time, 16)
SYSCALL2(sys_snooze, 17)
SYSCALL2(sys_sem_create, 18)
SYSCALL1(sys_sem_delete, 19)
SYSCALL2(sys_sem_acquire, 20)
SYSCALL5(sys_sem_acquire_etc, 21)
SYSCALL2(sys_sem_release, 22)
SYSCALL3(sys_sem_release_etc, 23)
SYSCALL0(sys_get_current_thread_id, 24)
SYSCALL1(sys_exit, 25)
SYSCALL3(sys_proc_create_proc, 26)
SYSCALL2(sys_thread_wait_on_thread, 27)
SYSCALL2(sys_proc_wait_on_proc, 28)
SYSCALL6(sys_vm_create_anonymous_region, 29)
SYSCALL5(sys_vm_clone_region, 30);
//SYSCALL0(sys_vm_mmap_file, 31);
SYSCALL1(sys_vm_delete_region, 32);
SYSCALL2(sys_vm_get_region_info, 33);
SYSCALL3(sys_thread_create_thread, 34);
SYSCALL1(sys_thread_kill_thread, 35);
SYSCALL1(sys_thread_suspend_thread, 36);
SYSCALL1(sys_thread_resume_thread, 37);
SYSCALL1(sys_proc_kill_proc, 38);
SYSCALL0(sys_get_current_proc_id, 39);

/* we will optimize by just passing a ptr to the place where the caller
** would have dumped the args */

syscall0:
	movl	$0, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall1:
	movl	$1, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall2:
	movl	$2, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall3:
	movl	$3, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall4:
	movl	$4, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall5:
	movl	$5, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall6:
	movl	$6, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall7:
	movl	$7, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall8:
	movl	$8, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall9:
	movl	$9, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall10:
	movl	$10, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall11:
	movl	$11, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall12:
	movl	$12, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall13:
	movl	$13, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall14:
	movl	$14, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall15:
	movl	$15, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

syscall16:
	movl	$16, %ecx
	lea		4(%esp), %edx
	int		$99
	ret

