/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
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

syscall0:
	int		$99
	ret	

syscall1:
	pushl	%ebx
	movl	8(%esp), %ebx
	int		$99
	popl	%ebx
	ret

syscall2:
	pushl	%ebx
	movl	8(%esp), %ebx
	movl	12(%esp), %ecx
	int		$99
	popl	%ebx
	ret
	
syscall3:
	pushl	%ebx
	movl	8(%esp), %ebx
	movl	12(%esp), %ecx
	movl	16(%esp), %edx
	int		$99
	popl	%ebx
	ret

syscall4:
	pushl	%ebx
	pushl	%esi
	movl	12(%esp), %ebx
	movl	16(%esp), %ecx
	movl	20(%esp), %edx
	movl	24(%esp), %esi
	int		$99
	popl	%esi
	popl	%ebx
	ret

syscall5:
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	16(%esp), %ebx
	movl	20(%esp), %ecx
	movl	24(%esp), %edx
	movl	28(%esp), %esi
	movl	32(%esp), %edi
	int		$99
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

syscall6:
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	movl	20(%esp), %ebx
	movl	24(%esp), %ecx
	movl	28(%esp), %edx
	movl	32(%esp), %esi
	movl	36(%esp), %edi
	movl	40(%esp), %ebp
	int		$99
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	ret
