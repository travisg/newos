	.file "sparcbootblock.S"

#define	STAGE2	0x381278

	.text
	.globl  start

start:
	set	STAGE2, %g1
	jmp	%g1
	nop

