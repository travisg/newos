/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#define	FUNC(name) .align 2 ; .globl _##name ; .type _##name,@function ; _##name

#if 0
FUNC(atomic_add):
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

FUNC(atomic_and):
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

FUNC(atomic_or):
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

FUNC(atomic_set):
	mov.l	r8,@-r15
	sts.l	pr,@-r15

	/* disable interrupts */
	mov.l	disable_interrupts_addr,r1
	jsr	@r1
	nop

	/* load the value, save it, and store the new value */
	mov.l	@r4,r8
	mov.l	r5,@r4

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

/* int test_and_set(int *val, int set_to, int test_val) */
FUNC(test_and_set):
	mov.l	r8,@-r15
	sts.l	pr,@-r15

	/* disable interrupts */
	mov.l	disable_interrupts_addr,r1
	jsr	@r1
	nop

	/* load the value, save it, and store the new value */
	mov.l	@r4,r8         /* load the dest, it will be the return value */
	cmp/eq	r8,r6          /* compare against the test_val */
	bf	_not_equal

	mov.l	r5,@r4         /* put the set_to value into the target */

_not_equal:
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
#endif


// void sh4_switch_stack_and_call(addr stack, void (*func)(void *), void *arg);
FUNC(sh4_switch_stack_and_call):
	mov		r4,r15
	jsr		@r5
	mov		r6,r4


