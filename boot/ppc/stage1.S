/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define BASE 0x100000
#define BOOTDIR_BASE (BASE + 0x1000)
#define STAGE2_BOOTDIR_PAGE	(BOOTDIR_BASE + 0x60)
#define STAGE2_OFFSET (BOOTDIR_BASE + 0x74)

.text
.globl _start
_start:
	lis		1,tempstack_end@ha		/* load the new stack */
	ori		1,1,tempstack@l

	/* load the base of the bootdir */
	lis		3,BOOTDIR_BASE@ha
	ori		3,3,BOOTDIR_BASE@l	

	/* load the offset the stage2 will start into the bootdir */
	lis		4,STAGE2_BOOTDIR_PAGE@ha
	ori		4,4,STAGE2_BOOTDIR_PAGE@l
	lwz		4,0(4)
	mulli	4,4,4096
	
	/* load the offset into that page the stage2 entry point will be */
	lis		5,STAGE2_OFFSET@ha
	ori		5,5,STAGE2_OFFSET@l
	lwz		5,0(5)

	/* add them all together */
	add		6,3,4
	add		6,6,5

	/* jump there */
	mtlr	6
	blr

.align 4
tempstack:
	.skip 0x800	
tempstack_end:
	
.data
#include "boot/ppc/final.asminclude"

