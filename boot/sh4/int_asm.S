.text
.globl _trap
.globl _vector_base
	
.align 1
_trap:
	trapa	#5
	rts
	nop
.align 1
_vector_base:
	.skip 0x100	
general_exception_handler:
	mov.l	is,r15
	mov.l	r0,@-r15
	mov.l	r1,@-r15
	mov.l	r2,@-r15
	mov.l	r3,@-r15
	mov.l	r4,@-r15
	mov.l	r5,@-r15
	mov.l	r6,@-r15
	mov.l	r7,@-r15
	mov.l	r8,@-r15
	mov.l	r9,@-r15
	mov.l	r10,@-r15
	mov.l	r11,@-r15
	mov.l	r12,@-r15
	mov.l	r13,@-r15
	mov.l	r14,@-r15
	sts.l	pr,@-r15

	mov	r15,r4	
	mov.l	ih1,r1
	jsr	@r1	
	nop

	lds.l	@r15+,pr
	mov.l	@r15+,r14	
	mov.l	@r15+,r13	
	mov.l	@r15+,r12	
	mov.l	@r15+,r11
	mov.l	@r15+,r10
	mov.l	@r15+,r9
	mov.l	@r15+,r8
	mov.l	@r15+,r7
	mov.l	@r15+,r6
	mov.l	@r15+,r5
	mov.l	@r15+,r4
	mov.l	@r15+,r3
	mov.l	@r15+,r2
	mov.l	@r15+,r1
	mov.l	@r15+,r0
	stc 	sgr,r15
	rte
	nop
general_exception_handler_end:
	.skip 0x300-(general_exception_handler_end-general_exception_handler)
mmu_exception_handler:
	mov.l	is,r15
	mov.l	r0,@-r15
	mov.l	r1,@-r15
	mov.l	r2,@-r15
	mov.l	r3,@-r15
	mov.l	r4,@-r15
	mov.l	r5,@-r15
	mov.l	r6,@-r15
	mov.l	r7,@-r15
	mov.l	r8,@-r15
	mov.l	r9,@-r15
	mov.l	r10,@-r15
	mov.l	r11,@-r15
	mov.l	r12,@-r15
	mov.l	r13,@-r15
	mov.l	r14,@-r15
	sts.l	pr,@-r15

	mov	r15,r4	
	mov.l	ih1,r1
	jsr	@r1	
	nop

	lds.l	@r15+,pr
	mov.l	@r15+,r14	
	mov.l	@r15+,r13	
	mov.l	@r15+,r12	
	mov.l	@r15+,r11
	mov.l	@r15+,r10
	mov.l	@r15+,r9
	mov.l	@r15+,r8
	mov.l	@r15+,r7
	mov.l	@r15+,r6
	mov.l	@r15+,r5
	mov.l	@r15+,r4
	mov.l	@r15+,r3
	mov.l	@r15+,r2
	mov.l	@r15+,r1
	mov.l	@r15+,r0
	stc sgr,r15
	rte
	nop

.align 2
is:
	.long	_int_stack+0x200-4
ih1:
	.long	_int_handler
	
.data
.align 2
_int_stack:
	.skip 0x200
_int_stack_end:
	.long 0

