/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
.text
.align 2

.globl _vector_base
_vector_base:
	.skip	0x100
exception_ent_1:
	mov.l	expevt_addr1,r0
	mov.l	@r0,r0
	shlr2	r0
	shlr	r0			/* shift the exception code over 3 bits */

	cmp/eq	#0x10,r0		/* test if it's a initial page write */
	bt	_tlb_ipw
	nop

	/* not a inital page write exception */
	/* just enter the kernel and let it deal with it */
	bra switch_banks_and_enter_kernel	
	nop

_tlb_ipw:
	/* deal with inital page write exception */
	/* we can stay on reg bank 1, switch to a tlb stack */
	mov.l	tlb_stack_addr1,r15

	/* save some of the floating point registers */
	/* they seem to be used by some of the libgcc stuff */
	/* saving: fpscr, fpul, dr0, dr2, dr4 */
	fmov.s	fr0,@-r15
	fmov.s	fr1,@-r15
	fmov.s	fr2,@-r15
	fmov.s	fr3,@-r15
	fmov.s	fr4,@-r15
	fmov.s	fr5,@-r15
	sts.l	fpul,@-r15
	sts.l	fpscr,@-r15

	mov	r0,r4			/* arg 1, excode */
	shlr2	r4			/* shift arg 1 over two more bits */
	mov.l	r4,@-r15		/* save the excode for after the call */
	sts.l	pr,@-r15		/* save the pr reg */
	stc	spc,r5			/* arg 2, spc */
	mov.l	initial_page_write_handler,r1
	jsr	@r1
	nop

	/* restore regs */
	lds.l	@r15+,pr
	mov.l	@r15+,r1
	lds.l	@r15+,fpscr
	lds.l	@r15+,fpul
	fmov.s	@r15+,fr5
	fmov.s	@r15+,fr4
	fmov.s	@r15+,fr3
	fmov.s	@r15+,fr2
	fmov.s	@r15+,fr1
	fmov.s	@r15+,fr0

	/* see if the tlb handler returned another exception code */
	/* if so, the tlb ipw just elevated to a page fault or other such */
	/* 'soft' faults. */

	cmp/eq	r0,r1
	bf	_soft_fault
	nop

	/* return from the tlb miss */
	stc	sgr,r15
	rte
	nop
	
_soft_fault:
	/* the tlb ipw handler returned another exception code */
	/* we now need to enter the kernel with the soft fault */
	shll2	r0
	mov.l	tea_addr1,r1
	mov.l	@r1,r1		/* load the faulted address */
	bra switch_banks_and_enter_kernel
	nop

.align 2
tea_addr1:	.long	0xff00000c
expevt_addr1:	.long	0xff000024
initial_page_write_handler:	.long	_tlb_initial_page_write
tlb_stack_addr1:	.long	tlb_stack_end-4

exception_ent_1_end:
	.skip 0x300-(exception_ent_1_end-exception_ent_1)
TLB_miss_ent:
	mov.l	expevt_addr2,r4
	mov.l	@r4,r4
	shlr2	r4
	shlr2	r4
	shlr	r4			/* shift the exception code over 5 bits */
	
	/* we can stay on reg bank 1, switch to a tlb stack */
	mov.l	tlb_stack_addr2,r15

	/* save some of the floating point registers */
	/* they seem to be used by some of the libgcc stuff */
	/* saving: fpscr, fpul, dr0, dr2, dr4 */
	fmov.s	fr0,@-r15
	fmov.s	fr1,@-r15
	fmov.s	fr2,@-r15
	fmov.s	fr3,@-r15
	fmov.s	fr4,@-r15
	fmov.s	fr5,@-r15
	sts.l	fpul,@-r15
	sts.l	fpscr,@-r15

	mov.l	r4,@-r15		/* save the exception code */
	sts.l	pr,@-r15		/* save the pr reg */

	/* arg 1 is exception code already in r4 */
	stc	spc,r5			/* arg 2, spc */
	mov.l	tlb_miss_handler,r1
	jsr	@r1
	nop

	/* see if the tlb miss handler returned another exception code */
	/* if so, the tlb miss just elevated to a page fault or other such */
	/* 'soft' faults. */
	lds.l	@r15+,pr		/* restore pr */
	mov.l	@r15+,r1		/* restore the original exception code */	

	/* restore the saved floating point registers */
	lds.l	@r15+,fpscr
	lds.l	@r15+,fpul
	fmov.s	@r15+,fr5
	fmov.s	@r15+,fr4
	fmov.s	@r15+,fr3
	fmov.s	@r15+,fr2
	fmov.s	@r15+,fr1
	fmov.s	@r15+,fr0

	cmp/eq	r0,r1			/* check against the stored original exception code */
	bf	_soft_fault1
	nop

	/* return from the tlb miss */
	stc	sgr,r15
	rte
	nop
	
_soft_fault1:
	/* the tlb miss handler returned another exception code */
	/* we now need to enter the kernel with the soft fault */

	shll2	r0
	mov.l	tea_addr2,r1
	mov.l	@r1,r1		/* load the faulted address */
	bra switch_banks_and_enter_kernel	
	nop

.align 2
tea_addr2:	.long	0xff00000c
expevt_addr2:	.long	0xff000024
tlb_miss_handler:	.long	_tlb_miss
tlb_stack_addr2:	.long	tlb_stack_end-4

TLB_miss_ent_end:	
	.skip 0x200-(TLB_miss_ent_end-TLB_miss_ent)
interrupt_ent:
	mov.l	intevt_addr,r0
	mov.l	@r0,r0
	shlr2	r0
	shlr	r0			/* shift the exception code over 3 bits */

	/* disable interrupts */
	mov.l	imask,r1
	stc	sr,r2
	or	r1,r2
	ldc	r2,sr	

switch_banks_and_enter_kernel:
	/* save the saved registers into memory */
	mov.l	save_stack,r2	
	stc.l	spc,@-r2
	stc.l	ssr,@-r2
	stc.l	sgr,@-r2
	mov.l	r0,@-r2			/* put the modified exception code there too */
	mov.l	r1,@-r2			/* put the saved page fault address on the stack */
	
	/* see if we need to load the kernel stack or stay on the current one */
	stc	ssr,r0
	mov.l	md_bit_mask,r1
	and	r1,r0
	cmp/eq	r1,r0
	bt	_keep_current_stack
	nop

	/* we need to set the stack because we came from user space */
	mov.l	kstack,r15
	bra	_have_set_stack
	nop

_keep_current_stack:
	stc	sgr,r15

_have_set_stack:
	/* enable exceptions & swap banks back to 0 */
	mov.l	bl_rb_bit_mask,r0
	stc	sr,r1
	and	r0,r1
	ldc	r1,sr
	
	/* at this point, we may take a TLB exception */

	/* start pushing registers */
	mov.l	r8,@-r15
	mov.l	r9,@-r15
	mov.l	r10,@-r15
	mov.l	r11,@-r15
	mov.l	r12,@-r15
	mov.l	r13,@-r15
	mov.l	r14,@-r15
	
	/* push r0-r7 */
	mov.l	r0,@-r15
	mov.l	r1,@-r15
	mov.l	r2,@-r15
	mov.l	r3,@-r15
	mov.l	r4,@-r15
	mov.l	r5,@-r15
	mov.l	r6,@-r15
	mov.l	r7,@-r15

_after_r0r7_save:
	/* save the floating point registers */
	/* XXX see about optimizing this later */
	fmov.s	fr0,@-r15
	fmov.s	fr1,@-r15
	fmov.s	fr2,@-r15
	fmov.s	fr3,@-r15
	fmov.s	fr4,@-r15
	fmov.s	fr5,@-r15
	fmov.s	fr6,@-r15
	fmov.s	fr7,@-r15
	fmov.s	fr8,@-r15
	fmov.s	fr9,@-r15
	fmov.s	fr10,@-r15
	fmov.s	fr11,@-r15
	fmov.s	fr12,@-r15
	fmov.s	fr13,@-r15
	fmov.s	fr14,@-r15
	fmov.s	fr15,@-r15
	frchg
	fmov.s	fr0,@-r15
	fmov.s	fr1,@-r15
	fmov.s	fr2,@-r15
	fmov.s	fr3,@-r15
	fmov.s	fr4,@-r15
	fmov.s	fr5,@-r15
	fmov.s	fr6,@-r15
	fmov.s	fr7,@-r15
	fmov.s	fr8,@-r15
	fmov.s	fr9,@-r15
	fmov.s	fr10,@-r15
	fmov.s	fr11,@-r15
	fmov.s	fr12,@-r15
	fmov.s	fr13,@-r15
	fmov.s	fr14,@-r15
	fmov.s	fr15,@-r15
	frchg
	sts.l	fpul,@-r15
	sts.l	fpscr,@-r15

	/* can save most of the special registers we need in r8-r14 */
	/* the abi we're working with saves r8-14 on function calls */
	stc.l	gbr,@-r15
	sts.l	mach,@-r15
	sts.l	macl,@-r15
	sts.l	pr,@-r15
	mov.l	saved_sgr,r12
	mov.l	r12,@-r15
	mov.l	saved_ssr,r12
	mov.l	r12,@-r15
	mov.l	saved_spc,r12
	mov.l	r12,@-r15
	mov.l	saved_excode,r12
	shlr2	r12
	mov.l	r12,@-r15
	mov.l	saved_pfault,r12
	mov.l	r12,@-r15

	/* arg1: address of the iframe */
	mov		r15,r4

	/* jump through the vector table into the kernel */
	mov.l	vector_table_addr,r0
	mov.l	saved_excode,r2
	mov.l	@(r0,r2),r1
	jsr	@r1
	nop

	/* restore everything and get outta here */
	add		#0x8,r15		/* pop the pfault and excode data from the stack */
	mov.l	saved_spc_addr,r1
	mov.l	@r15+,r2
	mov.l	r2,@r1
	mov.l	saved_ssr_addr,r1
	mov.l	@r15+,r2
	mov.l	r2,@r1
	mov.l	saved_sgr_addr,r1
	mov.l	@r15+,r2
	mov.l	r2,@r1
	lds.l	@r15+,pr
	lds.l	@r15+,macl
	lds.l	@r15+,mach
	ldc.l	@r15+,gbr	


	/* restore the floating point registers */
	lds.l	@r15+,fpscr
	lds.l	@r15+,fpul
	frchg
	fmov.s	@r15+,fr15
	fmov.s	@r15+,fr14
	fmov.s	@r15+,fr13
	fmov.s	@r15+,fr12
	fmov.s	@r15+,fr11
	fmov.s	@r15+,fr10
	fmov.s	@r15+,fr9
	fmov.s	@r15+,fr8
	fmov.s	@r15+,fr7
	fmov.s	@r15+,fr6
	fmov.s	@r15+,fr5
	fmov.s	@r15+,fr4
	fmov.s	@r15+,fr3
	fmov.s	@r15+,fr2
	fmov.s	@r15+,fr1
	fmov.s	@r15+,fr0
	frchg
	fmov.s	@r15+,fr15
	fmov.s	@r15+,fr14
	fmov.s	@r15+,fr13
	fmov.s	@r15+,fr12
	fmov.s	@r15+,fr11
	fmov.s	@r15+,fr10
	fmov.s	@r15+,fr9
	fmov.s	@r15+,fr8
	fmov.s	@r15+,fr7
	fmov.s	@r15+,fr6
	fmov.s	@r15+,fr5
	fmov.s	@r15+,fr4
	fmov.s	@r15+,fr3
	fmov.s	@r15+,fr2
	fmov.s	@r15+,fr1
	fmov.s	@r15+,fr0
	
	mov.l	@r15+,r7
	mov.l	@r15+,r6
	mov.l	@r15+,r5
	mov.l	@r15+,r4
	mov.l	@r15+,r3
	mov.l	@r15+,r2
	mov.l	@r15+,r1
	mov.l	@r15+,r0

_after_r0r7_restore:
	/* We need to calculate a new sr with exceptions off & register bank 1*/
	mov.l	bl_rb_bit_mask,r8
	not	r8,r8
	stc	sr,r9
	or	r9,r8
	mov.l	modified_sr_addr,r9	
	mov.l	r8,@r9

	/* pop the last few registers */
	mov.l	@r15+,r14
	mov.l	@r15+,r13
	mov.l	@r15+,r12
	mov.l	@r15+,r11
	mov.l	@r15+,r10
	mov.l	@r15+,r9
	mov.l	@r15+,r8

	/* now we only have r15 to use */
	mov.l	modified_sr,r15
	ldc	r15,sr
	
	/* restore the ssr & spc registers */
	mov.l	saved_spc,r15
	ldc	r15,spc
	mov.l	saved_ssr,r15
	ldc	r15,ssr
	mov.l	saved_sgr,r15
	
	/* get out of here */
	rte
	nop

.align 2
modified_sr_addr:	.long	modified_sr
vector_table_addr: .long	vector_table
trap_exception:	.long	0x2c
vector_base_addr: .long	_vector_base
md_bit_mask:	.long	0x40000000
bl_rb_bit_mask:	.long	0xcfffffff
save_stack:	.long	saved_spc+4
saved_page_fault_addr: .long	saved_pfault
saved_excode_addr: .long	saved_excode
saved_spc_addr:	.long	saved_spc
saved_ssr_addr:	.long	saved_ssr
saved_sgr_addr:	.long	saved_sgr
intevt_addr:	.long	0xff000028
tra_addr:	.long	0xff000020
imask:		.long	0x000000f0

/* the next memory addresses are used for temporary storage or
   data structures for the kernel to write in. They're in the
   text segment, I know, but it's much faster if we can reference
   them pcrel. */
saved_pfault:	.long	0
saved_excode:	.long	0
saved_sgr:	.long	0
saved_ssr:	.long	0
saved_spc:	.long	0		/* these four addresses are used to
					save the saved registers while we switch to another
					stack and register set */	
modified_sr:	.long	0
/* These memory locations are written to by the kernel */
.globl _kernel_struct
_kernel_struct:
kernel_pgdir:	.long	0
user_pgdir:	.long	0
kernel_asid:	.long	0
user_asid:	.long 	0
kstack:		.long	0
vector_table:
	.rep	0x100
	.long	0
	.endr

.data
.align 2
tlb_stack:
	.rep	0x1000
	.long	0
	.endr
tlb_stack_end:
