/*
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define FUNCTION(x) .global x; .type x,@function; x

/* everything between int86_code_start and int86_code_stop gets
 * copied to 0x7c00 before being called
 */
.globl int86_code_start
int86_code_start:

/*
 * struct regs {
 *	unsigned int eax;
 *	unsigned int ebx;
 *	unsigned int ecx;
 *	unsigned int edx;
 *	unsigned int esi;
 *	unsigned int edi;
 * };
 *
 * void int86(int interrupt, struct regs *r);
 */
FUNCTION(int86):
	pushal
	pushfl

	cli

	/* copy ourselves to 0x7c00 */
	movl	$0x7c00,%edi				/* destination */
	movl	$int86_code_start,%esi	/* source */
	movl	$int86_code_stop-int86_code_start,%ecx	/* count */
	cld
	rep movsb

	/* set the idt back to zero */
	lidt 	idt_descriptor

	/* get the interrupt vector and patch the instruction at the target address */
	movl	40(%esp),%eax
	mov		%al,(intinstruction+1-int86_code_start+0x7c00)

	/* load the regs from the passed-in structure */
	movl	44(%esp),%ebp 			/* ebp will hold the structure */
									/* dont get eax yet */
	movl	4(%ebp),%ebx
	movl	8(%ebp),%ecx
	movl	12(%ebp),%edx
	movl	16(%ebp),%esi
	movl	20(%ebp),%edi
	mov		24(%ebp),%es
	movl	(%ebp),%ebp				/* this value will go to %eax before the int call */

	/* save esp */
	movl	%esp,saved_esp

	/* get a new stack */
	movl	$0x7c00,%esp

	/* switch down to the low-memory version of this code */
	jmp		.foo-int86_code_start+0x7c00
.foo:

	/* unset the protected mode and paging bits */
	movl	%cr0,%eax
	andl	$0x7ffffffe,%eax
	movl	%eax,%cr0

	/* switch into 16-bit code */
	ljmp	$0x7c0,$real-int86_code_start
real:
.code16
	xor		%eax,%eax
	mov		%ax,%ds
	mov		%ax,%fs
	movw	$0xb800,%ax
	mov		%ax,%gs

	/* load %eax */
	movl	%ebp,%eax

	/* the int call */
intinstruction:
	.byte	0xcd
	.byte	0

	/* save the flags (we care about the carry bit) */
	pushf

	/* save %eax from the call */
	mov		%eax,%ebp	

	/* set the protected mode and paging bit */
	movl	%cr0,%eax
	orl		$0x80000001,%eax
	movl	%eax,%cr0

.code32
	.byte	0x66
	ljmp	$0x8,$protected

protected:
	mov		$0x10,%ax
	mov		%ax,%ds
	mov		%ax,%es
	mov		%ax,%fs
	mov		%ax,%gs

	/* restore the stack */
	movl	saved_esp,%esp

	/* save the registers from the call */
	movl	44(%esp),%eax
	movl	%ebp,(%eax)
	movl	%ebx,4(%eax)
	movl	%ecx,8(%eax)
	movl	%edx,12(%eax)
	movl	%esi,16(%eax)
	movl	%edi,20(%eax)
	movw	0x7bfe,%cx
	movw	%cx,24(%eax)

	popfl
	popal

	ret

idt_descriptor:
	.short	0x7ff
	.long	0x0

saved_esp:
	.long	0

.globl int86_code_stop
int86_code_stop:
	.long	0

