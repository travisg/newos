/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#define	FUNC(name) .align 2 ; .globl _##name ; _##name:

.text
FUNC(reboot)
	mov.l	disable_exceptions_addr,r1
	jsr	@r1	
	nop
	mov	#0,r0
	mov.l	r1,@r0
	trapa	#5			/* trap, thus causing a reset */
	nop
	/* no return */

.align 2
disable_exceptions_addr:	.long	_disable_exceptions

FUNC(atomic_add)
	mov.l	r8,@-r15
	sts.l	pr,@-r15

	/* disable interrupts */
	mov.l	disable_interrupts_addr,r1
	jsr	@r1
	nop

	/* load the value, save it, add to it, and store it back */
	mov.l	@r4,r3
	mov	r3,r8
	add	r5,r3
	mov.l	r3,@r4

	/* restore interrupts */
	mov.l	restore_interrupts_addr,r1
	jsr	@r1
	mov	r0,r4
	
	/* return value will be old value */
	mov	r8,r0

	/* restore the stack */
	lds.l	@r15+,pr
	rts
	mov.l	@r15+,r8

FUNC(atomic_and)
	mov.l	r8,@-r15
	sts.l	pr,@-r15

	/* disable interrupts */
	mov.l	disable_interrupts_addr,r1
	jsr	@r1
	nop

	/* load the value, save it, and it, and store it back */
	mov.l	@r4,r3
	mov	r3,r8
	and	r5,r3
	mov.l	r3,@r4

	/* restore interrupts */
	mov.l	restore_interrupts_addr,r1
	jsr	@r1
	mov	r0,r4
	
	/* return value will be old value */
	mov	r8,r0

	/* restore the stack */
	lds.l	@r15+,pr
	rts
	mov.l	@r15+,r8

FUNC(atomic_or)
	mov.l	r8,@-r15
	sts.l	pr,@-r15

	/* disable interrupts */
	mov.l	disable_interrupts_addr,r1
	jsr	@r1
	nop

	/* load the value, save it, or it, and store it back */
	mov.l	@r4,r3
	mov	r3,r8
	or	r5,r3
	mov.l	r3,@r4

	/* restore interrupts */
	mov.l	restore_interrupts_addr,r1
	jsr	@r1
	mov	r0,r4
	
	/* return value will be old value */
	mov	r8,r0

	/* restore the stack */
	lds.l	@r15+,pr
	rts
	mov.l	@r15+,r8

/* XXX finish */
FUNC(test_and_set)
	rts
	nop

.align 2
disable_interrupts_addr:	.long	_arch_int_disable_interrupts
restore_interrupts_addr:	.long	_arch_int_restore_interrupts

FUNC(disable_exceptions)
	mov.l	bl_bit_mask,r0
	stc	sr,r1
	or	r0,r1
	ldc	r1,sr			/* turn off interrupts/exceptions */		
	rts
	nop
	
FUNC(enable_exceptions)
	mov.l	bl_bit_mask,r0
	not	r0,r0
	stc	sr,r1
	and	r0,r1
	ldc	r1,sr
	rts
	nop

.align 2
bl_bit_mask:	.long	0x10000000

FUNC(arch_int_restore_interrupts)
	mov.l	inverse_imask_bit_mask,r0
	stc	sr,r1			/* get the sr register */
	and	r0,r1			/* zero out the imask part */
	or	r4,r1			/* or in the passed in imask, should only contain imask bits */
	ldc	r1,sr			/* put the new status into the sr register */
	rts
	nop
	
FUNC(arch_int_enable_interrupts)
	mov.l	inverse_imask_bit_mask,r0
	stc	sr,r1			/* load the sr register */
	and	r0,r1			/* set the imask to 0 */
	ldc	r1,sr			/* put the new status into the sr register */
	rts
	nop

.align 2
inverse_imask_bit_mask:	.long	0xffffff0f

FUNC(arch_int_disable_interrupts)
	mov.l	imask_bit_mask,r2
	stc	sr,r1			/* load the sr register */
	mov	r1,r0			/* save the old sr register */
	or	r2,r1			/* or in 0xf for the imask */	
	ldc	r1,sr			/* set the new sr register with the interrupts masked */
	rts
	and	r2,r0			/* make sure the return value contains only the imask part */

.align 2
imask_bit_mask:	.long	0x000000f0

FUNC(get_sr)
	stc	sr,r0
	rts
	nop

FUNC(get_fpscr)
	sts	fpscr,r0
	rts
	nop

// void sh4_context_switch(unsigned int **old_esp, unsigned int *new_esp);
FUNC(sh4_context_switch)
	fmov.s	fr12,@-r15
	fmov.s	fr13,@-r15
	fmov.s	fr14,@-r15
	fmov.s	fr15,@-r15
	sts.l	fpscr,@-r15
	mov.l	r8,@-r15
	mov.l	r9,@-r15
	mov.l	r10,@-r15
	mov.l	r11,@-r15
	mov.l	r12,@-r15
	mov.l	r13,@-r15
	mov.l	r14,@-r15
	sts.l	pr,@-r15

	mov.l	r15,@r4
	mov	r5,r15

	lds.l	@r15+,pr
	mov.l	@r15+,r14
	mov.l	@r15+,r13
	mov.l	@r15+,r12
	mov.l	@r15+,r11
	mov.l	@r15+,r10
	mov.l	@r15+,r9
	mov.l	@r15+,r8
	lds.l	@r15+,fpscr
	fmov.s	@r15+,fr15
	fmov.s	@r15+,fr14
	fmov.s	@r15+,fr13
	fmov.s	@r15+,fr12

	rts
	nop

FUNC(sh4_function_caller)
_fc_loop:
	mov.l	@r15+,r2
	jsr	@r2
	nop
	bra	_fc_loop
	nop

// void sh4_enter_uspace(addr entry, addr ustack_top);
FUNC(sh4_enter_uspace)
	ldc		r4,spc		// load the program counter it will switch to
	mov		r5,r15		// restore the user stack
	mov.l	uspace_sr,r0
	ldc		r0,ssr
	rte
	nop

uspace_sr:	.long	0x00000000

