/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define FUNCTION(x) .global x; .type x,@function; x

/* uint64 rdtsc() */
FUNCTION(rdtsc):
	rdtsc

	/* this is loaded into eax:edx, lets move edx into the top of rax */
	shl		$32,%rdx
	xor		%rax,%rdx

	ret

FUNCTION(execute_n_instructions):
	mov		%edi,%ecx
	shr		$4, %ecx
.again:
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	xor		%eax, %eax
	loop	.again
	ret

FUNCTION(system_time):
	// XXX fixme
#if 0
   	/* load 64-bit factor into %eax (low), %edx (high) */
   	/* hand-assemble rdtsc -- read time stamp counter */
	rdtsc		/* time in %edx,%eax */

	push	%ebx
	movl	cv_factor, %ebx
	movl	%edx, %ecx	/* save high half */
	mull	%ebx 		/* truncate %eax, but keep %edx */
	movl	%ecx, %eax
	movl	%edx, %ecx	/* save high half of low */
	mull	%ebx			/*, %eax*/
	/* now compute  [%edx, %eax] + [%ecx], propagating carry */
	subl	%ebx, %ebx	/* need zero to propagate carry */
	addl	%ecx, %eax
	adc		%ebx, %edx
	popl	%ebx
#endif
	ret

FUNCTION(cpuid):
 	push	%rbx

	mov		%edi,%eax		/* first arg, cpuid function */
	
 	cpuid
 	mov		%eax,0(%rsi)
 	mov		%ebx,4(%rsi)
 	mov		%ecx,8(%rsi)
 	mov		%edx,12(%rsi)

 	pop		%rbx
 	ret

FUNCTION(get_rflags):
 	pushfq
 	pop		%rax
 	ret

FUNCTION(set_rflags):
 	push	%edi
 	popfq
	ret

