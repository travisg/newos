/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/arch/string.h>

.globl memcpy

.align 4
memcpy:
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%edi	/* source */
	movl	16(%esp),%esi	/* dest */
	movl	20(%esp),%ecx	/* count */
	movl	16(%esp),%eax	/* retval */
	
	rep
	movsb
	
	popl	%edi
	popl	%esi
	ret