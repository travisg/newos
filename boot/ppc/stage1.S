/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define BASE 0x100000
#define BOOTDIR_BASE (BASE + 0x1000)
#define STAGE2_BOOTDIR_ENTRY	(BOOTDIR_BASE + 0x80)
#define STAGE2_BOOTDIR_PAGE	(STAGE2_BOOTDIR_ENTRY + 0x60)
#define STAGE2_OFFSET 		(STAGE2_BOOTDIR_ENTRY + 0x74)

.text
.globl _start
_start:
	lis		r1,tempstack_end@ha		/* load the new stack */
	ori		r1,r1,tempstack_end@l

	/* load the base of the bootdir */
	lis		r8,BOOTDIR_BASE@ha
	ori		r8,r8,BOOTDIR_BASE@l

	/* load the offset the stage2 will start into the bootdir */
	lis		r9,STAGE2_BOOTDIR_PAGE@ha
	ori		r9,r9,STAGE2_BOOTDIR_PAGE@l
	lwz		r9,0(r9)
	mulli	r9,r9,4096

	/* load the offset into that page the stage2 entry point will be */
	lis		r10,STAGE2_OFFSET@ha
	ori		r10,r10,STAGE2_OFFSET@l
	lwz		r10,0(r10)

	/* add them all together */
	add		r11,r8,r9
	add		r11,r11,r10

	/* jump there */
	mtlr	r11
	blr

.align 4
tempstack:
	.skip 0x800
tempstack_end:

.data
#include "build/ppc/boot/final.asminclude"

