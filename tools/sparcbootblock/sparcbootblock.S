/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
	.file "sparcbootblock.S"

#define	STAGE2	0x381278

	.text
	.globl  start

start:
	set	STAGE2, %g1
	jmp	%g1
	nop

