.text
.align 2

.globl _vector_base
_vector_base:
	.skip	0x100
exception_ent_1:
#if 0
        mov.l   is,r15
        mov.l   r0,@-r15
        mov.l   r1,@-r15
        mov.l   r2,@-r15
        mov.l   r3,@-r15
        mov.l   r4,@-r15
        mov.l   r5,@-r15
        mov.l   r6,@-r15
        mov.l   r7,@-r15
        mov.l   r8,@-r15
        mov.l   r9,@-r15
        mov.l   r10,@-r15
        mov.l   r11,@-r15
        mov.l   r12,@-r15
        mov.l   r13,@-r15
        mov.l   r14,@-r15
        sts.l   pr,@-r15

        mov     r15,r4
        mov.l   ih,r1
        jsr     @r1
        nop

.align 2
is:
        .long   _int_stack+0x200-4
ih:	.long	_default_vector
#endif
	mov.l	expevt_addr1,r0
	mov.l	@r0,r0
	shlr2	r0
	shlr	r0			/* shift the exception code over 3 bits */

	cmp/eq	#0x10,r0		/* test if it's a initial page write */
	bf	_is_regular_exception	
	nop
	/* XXX deal with inital page write exception */

_is_regular_exception:
	bra switch_banks_and_enter_kernel	
	nop

.align 2
expevt_addr1:	.long	0xff000024
exception_flag:	.long	handling_exception

exception_ent_1_end:
	.skip 0x300-(exception_ent_1_end-exception_ent_1)
TLB_miss_ent:
	
TLB_miss_ent_end:	
	.skip 0x200-(TLB_miss_ent_end-TLB_miss_ent)
interrupt_ent:
	mov.l	intevt_addr,r0
	mov.l	@r0,r0
	shlr2	r0
	shlr	r0			/* shift the exception code over 3 bits */

switch_banks_and_enter_kernel:
	/* save the saved registers into memory */
	mov.l	save_stack,r1	
	stc.l	spc,@-r1
	stc.l	ssr,@-r1
	stc.l	sgr,@-r1
	mov.l	r0,@-r1			/* put the modified exception code there too */
	
	/* enable exceptions & swap banks back to 0 */
	mov.l	bl_rb_bit_mask,r0
	stc	sr,r1
	and	r0,r1
	ldc	r1,sr
	
	/* at this point, we may take a TLB exception */

	/* load the kernel stack and start pushing registers */
	mov.l	kstack,r15
	mov.l	r8,@-r15
	mov.l	r9,@-r15
	mov.l	r10,@-r15
	mov.l	r11,@-r15
	mov.l	r12,@-r15
	mov.l	r13,@-r15
	mov.l	r14,@-r15
	
	/* check to see if this is a trap, in which case we dont push r0-r7 */
	mov.l	saved_excode,r8
	mov.l	trap_exception,r9
	cmp/eq	r9,r8
	bt	_after_r0r7_save
	nop

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
	/* can save most of the special registers we need in r8-r14 */
	/* the abi we're working with saves r8-14 on function calls */
	stc	gbr,r8
	sts	mach,r9
	sts	macl,r10
	sts	pr,r11
	mov.l	saved_sgr,r12
	mov.l	saved_ssr,r13
	mov.l	saved_spc,r14

	/* finish pushing the registers */
	mov.l	saved_excode,r4		/* arg 1, exception code */
	mov	r4,r2			/* save it for later */
	mov.l	r4,@-r15
	shlr2	r4			/* shift it two more bits to make exceptions be in range 0-255 */
	mov	r14,r5			/* arg 2, program counter */
	mov.l	tra_addr,r1
	mov.l	@r1,r6			/* arg 3, trap code, if it exists */

	/* jump through the vector table into the kernel */
	mov.l	vector_table_addr,r0
	mov.l	@(r0,r2),r1
	jsr	@r1
	nop

	/* restore everything and get outta here */
	mov.l	saved_spc_addr,r1
	mov.l	r14,@r1
	mov.l	saved_ssr_addr,r1
	mov.l	r13,@r1
	mov.l	saved_sgr_addr,r1
	mov.l	r12,@r1	
	lds	r11,pr
	lds	r10,macl
	lds	r9,mach
	ldc	r8,gbr	
	
	/* see if we need to restore r0-r7 */
	mov.l	@r15+,r0		/* get the exception code */
	cmp/eq	#0x2c,r0
	bt	_after_r0r7_restore
	nop

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
	mov.l	saved_ssr,r0
	ldc	r0,ssr
	mov.l	saved_spc,r0
	ldc	r0,spc
	mov.l	saved_sgr,r15
	
	/* get out of here */
	rte
	nop

.align 2
modified_sr_addr:	.long	modified_sr
modified_sr:	.long	0
vector_table_addr: .long	vector_table
trap_exception:	.long	0x2c
vector_base_addr: .long	_vector_base
bl_rb_bit_mask:	.long	0xcfffffff
expevt_addr2:	.long	0xff000024
save_stack:	.long	saved_spc+4
saved_excode_addr: .long	saved_excode
saved_spc_addr:	.long	saved_spc
saved_ssr_addr:	.long	saved_ssr
saved_sgr_addr:	.long	saved_sgr
saved_excode:	.long	0
saved_sgr:	.long	0
saved_ssr:	.long	0
saved_spc:	.long	0		/* these three addresses are used to
					save the saved registers while we switch to another
					stack and register set */	
intevt_addr:	.long	0xff000028
tra_addr:	.long	0xff000020

/* These memory locations are written to by the kernel */
.globl _kernel_struct
_kernel_struct:
kernel_pgdir:	.long	0
user_pgdir:	.long	0
kernel_asid:	.long	0
user_asid:	.long 	0
kstack:		.long	0
handling_exception:	.long	0
vector_table:
	.rep	0x100
	.long	0
	.endr

.data
.align 2
_int_stack:
        .skip 0x200
