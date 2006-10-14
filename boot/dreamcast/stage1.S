/*
** Copyright 2001-2006, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
/* addresses of values stored in the bootdir, which starts on the next
   page after this code. */
#define BASE 0x8c000000
#define BOOTDIR_BASE (BASE + 0x1000)

/* a couple of pointers into the second bootdir entry */
#define STAGE2_ENTRY (BOOTDIR_BASE + 0xf4)

.text
.globl start

start:
	/* find the offset into the stage2 bootloader where the entry point is */
	mov.l	entry_addr,r0
	mov.l	@r0,r0

	/* jump into it */
	jmp		@r0
	nop

.align 2
entry_addr:
	.long	STAGE2_ENTRY
