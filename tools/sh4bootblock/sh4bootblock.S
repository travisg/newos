#define ENTRY (0x8c000000 + 0x2080)
.text

start:
	mov.l	target,r1
	jmp	@r1
	nop

.align 2
target:
	.long	ENTRY
