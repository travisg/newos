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

FUNC(system_time)
	rts
	nop

FUNC(atomic_add)
	rts
	nop

FUNC(atomic_and)
	rts
	nop

FUNC(atomic_or)
	rts
	nop

FUNC(test_and_set)
	rts
	nop

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

