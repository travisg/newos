/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Michael Noisternig. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/arch/x86_64/descriptors.h>
#include <kernel/arch/x86_64/kernel.h>

#define FUNCTION(x) .global x; .type x,@function; x

.text

#define TRAP_ERRC(name, vector) \
.globl name; \
.type x,@function; \
.align 8; \
name: \
	pushq	$vector; \
	jmp		int_bottom

#define TRAP(name, vector) \
.globl name; \
.type x,@function; \
.align 8; \
name: \
	pushq	$0; \
	pushq	$vector; \
	jmp		int_bottom

TRAP(trap0, 0)
TRAP(trap1, 1)
TRAP(trap2, 2)
TRAP(trap3, 3)
TRAP(trap4, 4)
TRAP(trap5, 5)
TRAP(trap6, 6)
TRAP(trap7, 7)
TRAP_ERRC(trap8, 8)
TRAP(trap9, 9)
TRAP_ERRC(trap10, 10)
TRAP_ERRC(trap11, 11)
TRAP_ERRC(trap12, 12)
TRAP_ERRC(trap13, 13)
TRAP_ERRC(trap14, 14)
TRAP(trap16, 16)
TRAP_ERRC(trap17, 17)
TRAP(trap18, 18)
TRAP(trap19, 19)

TRAP(trap32, 32)
TRAP(trap33, 33)
TRAP(trap34, 34)
TRAP(trap35, 35)
TRAP(trap36, 36)
TRAP(trap37, 37)
TRAP(trap38, 38)
TRAP(trap39, 39)
TRAP(trap40, 40)
TRAP(trap41, 41)
TRAP(trap42, 42)
TRAP(trap43, 43)
TRAP(trap44, 44)
TRAP(trap45, 45)
TRAP(trap46, 46)
TRAP(trap47, 47)

TRAP(trap99, 99)

TRAP(trap251, 251)
TRAP(trap252, 252)
TRAP(trap253, 253)
TRAP(trap254, 254)
TRAP(trap255, 255)

.align 16
.globl int_bottom
int_bottom:
	/* push all the GP regs except rsp */
	push	%rax
	push	%rcx
	push	%rdx
	push	%rbx
	push	%rsp
	push	%rbp
	push	%rsi
	push	%rdi
	push	%r8
	push	%r9
	push	%r10
	push	%r11
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%fs
	push	%gs
	cld

	mov		%rsp, %rdi
	call	x86_64_handle_trap

	pop		%gs
	pop		%fs
	pop		%r15
	pop		%r14
	pop		%r13
	pop		%r12
	pop		%r11
	pop		%r10
	pop		%r9
	pop		%r8
	pop		%rdi
	pop		%rsi
	pop		%rbp
	pop		%rsp
	pop		%rbx
	pop		%rdx
	pop		%rcx
	pop		%rax

	add		$16,%rsp
	iret

/* syscall entry point. arg passing is as follows:
 * eax: vector number
 * ecx: number of words on stack
 * edx: pointer to args on user stack
 *
 * algorithm: push all registers, copy arguments to kernel stack, indirect
 * jump through syscall_table (in kernel/syscalls.c).
 */ 
.align 8
FUNCTION(x86_64_syscall_vector):
#warning implement x86_64_syscall_vector
#if 0
	/* push a standard iframe to the stack */
	pushl	$0		// error code 0
	pushl	$99		// vector 99 (syscall)
	pushl	%edx
	pushl	%eax
	pusha
	push	%ds
	push	%es
	push	%fs
	push	%gs
	movl	$KERNEL_DATA_SEG,%ebx
	cld
	movl	%ebx,%ds
	movl	%ebx,%es

	/* save the current kernel stack, so we can restore to here in case of any error */
	mov		%esp,%ebp

#if 0
	/* print the syscall */
	push	num_syscall_table_entries
	push	%edx
	push	%ecx
	push	%eax
	movl	%dr3,%ebx
	addl	$12,%ebx
	pushl	(%ebx)
	push	$syscall_entry_msg
	call	dprintf
	add		$8,%esp
	pop		%eax
	pop		%ecx
	pop		%edx
	add		$4,%esp
#endif

	/* sanity check vector number */
	test	%eax,%eax
	js		syscall_vector_error
	cmp		num_syscall_table_entries,%eax
	jge		syscall_vector_error

	/* sanity check number of args (must be 0-15) */
	test	$0xfffffff0,%ecx
	jnz		syscall_vector_error

	/* check to make sure the args pointer is not inside kernel space */
	test	$KERNEL_ADDR_MASK,%edx
	jnz		syscall_vector_error

	/* if the syscall has no args, fast path over the arg copy code */
	test	%ecx,%ecx
	jz		skip_arg_copy

 	/* set the fault handler in case we get an error copying data from the user stack */
	movl	%dr3,%ebx					// get the current thread structure
	mov		fault_handler_offset,%esi	// get the offset into the thread structure that fault_handler is
 	lea		(%ebx,%esi),%ebx
	mov		$syscall_vector_error,%esi
	mov		%esi,(%ebx)					// stick the error handler into t->fault_handler

	/* copy the appropriate number of args to the stack */
	shl		$2,%ecx
	sub		%ecx,%esp				// move the stack pointer down by the amount we are about to dump on it
	shr		$2,%ecx

	mov		%edx,%esi				// source is user space stack
	mov		%esp,%edi				// dest is kernel stack
	rep 
	movsl							// copy them

	movl	$0,(%ebx)				// unset the error handler	 

skip_arg_copy:
	movl	%eax,%esi				// move the vector number over to %esi

	/* push the current iframe into the iframe stack */
	pushl	%ebp
	call	x86_64_push_frame
	addl	$4,%esp

	/* do kernel work upon entry */
	call	thread_atkernel_entry

	/* load the syscall vector table */
	movl	$syscall_table,%ebx
	call	*(%ebx,%esi,4)

	/* save the return code into the iframe */
	movl	%edx,0x24(%ebp)
	movl	%eax,0x2c(%ebp)

	/* do any cleanup work when leaving the kernel */
	call	thread_atkernel_exit

	call	x86_64_pop_iframe

syscall_vector_exit:
	/* put the kernel stack back where it was before we pushed the args */
	mov		%ebp,%esp

	/* restore saved regs */
	pop		%gs
	pop		%fs
	pop		%es
	pop		%ds

	/* reverse the pusha from above */
	popa
	addl	$16,%esp // no need to pop orig_eax,orig_edx,vector,error_code
	iret

syscall_vector_error:
	xorl	$0xffffffff,%edx
	movl	%edx,0x24(%ebp)
	movl	$-4,0x2c(%ebp)
	jmp		syscall_vector_exit

syscall_entry_msg:
	.ascii	"syscall tid 0x%x eax %d ecx %d edx 0x%x max syscall 0x%x\n\0"
bleh:
	.ascii	"here 0x%x\n\0"

	.align	4
#endif

#warning make sure this matches the syscall mechanism
FUNCTION(x86_64_return_from_signal):
	add		$12, %rsp	// Flushes the 3 arguments to sa_handler
	mov		$81, %rax	// This syscall will restore the cpu context to the
	mov		$0, %rcx	// one existing before calling the signal handler
	int		$99
	ret
FUNCTION(x86_64_end_return_from_signal):

