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
