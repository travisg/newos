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
SYSCALL0(sys_exit, 17)

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
