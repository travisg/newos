/*
** Copyright 2005, Michael Noisternig. All rights reserved.
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define GLOBAL(name) \
	.globl name; \
	.type name,@function; \
	.align 8; \
	name

GLOBAL(memcpy):
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%edi	/* dest */
	movl	%edi,%eax	/* save dest ptr as return address */
	movl	16(%esp),%esi	/* source */
	movl	20(%esp),%ecx	/* count */

	cmpl	%edi,%esi
memcpy_1:
	je		memcpy_2

	movl	%ecx,%edx

	/* move by words */
	cld
	shrl	$2,%ecx
	rep
	movsl

	/* move any remaining data by bytes */
	movl	%edx,%ecx
	andl	$3,%ecx
	rep
	movsb

memcpy_2:
	popl	%edi
	popl	%esi
	ret

GLOBAL(memmove):
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%edi	/* dest */
	movl	%edi,%eax	/* save dest ptr as return address */
	movl	16(%esp),%esi	/* source */
	movl	20(%esp),%ecx	/* count */

	cmpl	%edi,%esi
	jge		memcpy_1

	std
	leal	-4(%edi,%ecx),%edi
	leal	-4(%esi,%ecx),%esi
	movl	%ecx,%edx

	/* move by words */
	shrl	$2,%ecx
	rep
	movsl

	/* move any remaining data by bytes */
	addl	$3,%edi
	addl	$3,%esi
	movl	%edx,%ecx
	andl	$3,%ecx
	rep
	movsb

	cld
	popl	%edi
	popl	%esi
	ret
