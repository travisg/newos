/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#define ENTRY (0x8c000000 + 0x2080)
.text

start:
	mov.l	target,r1
	jmp	@r1
	nop

.align 2
target:
	.long	ENTRY
