/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Michael Noisternig. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define FUNCTION(x) .global x; .type x,@function; x

.text

/* int atomic_add(int *val, int incr) */
FUNCTION(atomic_add):
	lock
	xadd	%esi,(%rdi)
	mov		%esi, %eax
	ret

/* int atomic_and(int *val, int incr) */
FUNCTION(atomic_and):
	mov		(%rdi),%eax
	mov		%esi,%ecx
	and		%eax,%ecx

	lock
	cmpxchg	%ecx,(%rdi)

	jnz		atomic_and

	ret

/* int atomic_or(int *val, int incr) */
FUNCTION(atomic_or):
	mov		(%rdi),%eax
	mov		%esi,%ecx
	or		%eax,%ecx

	lock
	cmpxchg	%ecx,(%rdi)

	jnz		atomic_or

	ret

/* int atomic_set(int *val, int set_to) */
FUNCTION(atomic_set):
	xchg	%esi,(%rdi)
	mov		%edi,%eax
	ret

/* int test_and_set(int *val, int set_to, int test_val) */
FUNCTION(test_and_set):
	mov		%edx,%eax

	lock
	cmpxchg	%esi,(%rdi)

	ret

/* uint64 x86_64_rdtsc() */
FUNCTION(x86_64_rdtsc):
x86_64_rdtsc:
	rdtsc

	/* rdtsc leaves us with a 64-bit value in edx:eax */
	shl		$32,%rdx
	or		%rdx,%rax	
	ret

/* void arch_cpu_global_TLB_invalidate(); */
FUNCTION(arch_cpu_global_TLB_invalidate):
	mov		%cr3,%rax
	mov		%rax,%cr3
	ret

/* void x86_64_fsave(void *fpu_state); */
FUNCTION(x86_64_fsave):
	fsave	(%rdi)
	ret

/* void x86_64_fxsave(void *fpu_state); */
FUNCTION(x86_64_fxsave):
	fxsave	(%rdi)
	ret

/* void x86_64_frstor(void *fpu_state); */
FUNCTION(x86_64_frstor):
	frstor	(%rdi)
	ret

/* void x86_64_fxrstor(void *fpu_state); */
FUNCTION(x86_64_fxrstor):
	fxrstor	(%rdi)
	ret

/* void x86_64_fsave_swap(void *old_fpu_state, void *new_fpu_state); */
FUNCTION(x86_64_fsave_swap):
	fsave	(%rdi)
	frstor	(%rsi)
	ret

/* void x86_64_fxsave_swap(void *old_fpu_state, void *new_fpu_state); */
FUNCTION(x86_64_fxsave_swap):
	fxsave	(%rdi)
	fxrstor	(%rsi)
	ret

/* void x86_64_context_switch(addr_t *old_sp, addr_t new_sp, addr_t new_pgdir); */
FUNCTION(x86_64_context_switch):
	/* push all callee-saved registers (rbp,rbx,r12-r15) */
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%rbp

	mov		%rsp,(%rdi)	/* save current_stack */
	mov		%rsi,%rsp	/* swap to the new stack */

	or		%rdx,%rdx	/* check new_pgdir against null */
	je		skip_pgdir_swap
		mov		%rdx,%cr3	/* swap memory context */
skip_pgdir_swap:

	pop		%rbp
	pop		%r15
	pop		%r14
	pop		%r13
	pop		%r12
	pop		%rbx

	ret

/* void x86_64_swap_pgdir(addr_t new_pgdir); */
FUNCTION(x86_64_swap_pgdir):
	mov		%rdi,%cr3
	ret

#warning make sure this matches the syscall calling convention
/* thread exit stub */
	.align 4
x86_64_uspace_exit_stub:
	push	%rax
	mov		$1, %rcx
	lea		(%rsp), %rdx
	mov		$25, %rax;
	int		$99
	.align 4
x86_64_uspace_exit_stub_end:

#warning implement x86_64_enter_uspace
/* void x86_64_enter_uspace(addr_t entry, void *args, addr_t ustack_top); */
FUNCTION(x86_64_enter_uspace):
	jmp .
#if 0
	movl	4(%esp),%eax	// get entry point
	movl	8(%esp),%edx	// get arguments
	movl	12(%esp),%ebx	// get user stack
	movw	$0x23,%cx
	movw	%cx,%ds
	movw	%cx,%es
	movw	%cx,%fs
	movw	%cx,%gs

	// copy exit stub to stack
	movl	$x86_64_uspace_exit_stub_end, %esi
_copy_more:
	lea	-4(%esi), %esi
	lea	-4(%ebx), %ebx
	mov	(%esi), %ecx
	mov	%ecx, (%ebx)
	cmp	$x86_64_uspace_exit_stub, %esi
	jg	_copy_more


	// push the args onto the user stack
	movl	%edx,-4(%ebx)	// args
	movl	%ebx,-8(%ebx)	// fake return address to copied exit stub
	sub		$8,%ebx

	pushl	$0x23			// user data segment
	pushl	%ebx			// user stack
	pushl	$(1 << 9) | 2	// user flags
	pushl	$0x1b			// user code segment
	pushl	%eax			// user IP
	iret
#endif

/* void x86_64_switch_stack_and_call(addr_t stack, void (*func)(void *), void *arg); */
FUNCTION(x86_64_switch_stack_and_call):
	mov		%rdi,%rsp		// switch the stack
	mov		%rdx,%rdi		// copy the argument
	call	*%rsi			// call the target function
_loop:
	jmp		_loop

#warning implement dbg_save_registers
FUNCTION(dbg_save_registers):
	ret
#if 0
	pushl	%esi
	pushl	%eax
	movl	12(%esp), %esi

	movl	%eax, 0(%esi)
	movl	%ebx, 4(%esi)
	movl	%ecx, 8(%esi)
	movl	%edx, 12(%esi)

	lea	16(%esp), %eax
	movl	%eax, 16(%esi)	// caller's %esp
	movl	%ebp, 20(%esi)

	movl	4(%esp), %eax
	movl	%eax, 24(%esi)	// caller's %esi
	movl	%edi, 28(%esi)

	movl	8(%esp), %eax
	movl	%eax, 32(%esi)	// caller's %ebp


	pushfl
	popl	%eax
	mov	%eax, 36(%esi)

	movl	%cs, 40(%esi)
	movl	%ss, 44(%esi)
	movl	%ds, 48(%esi)
	movl	%es, 52(%esi)

	popl	%eax
	popl	%esi
	ret
#endif
