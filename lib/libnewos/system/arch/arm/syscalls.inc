/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define SYSCALL0(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL1(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL2(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL3(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL4(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL5(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL6(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr


#define SYSCALL7(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL8(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL9(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL10(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL11(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL12(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL13(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL14(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL15(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr

#define SYSCALL16(name, n) \
.globl name; \
.type name,function; \
.align 8; \
name: \
	swi		$n; \
	mov		pc,lr



#if 0
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
#endif


