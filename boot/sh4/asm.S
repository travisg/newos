.text
.globl _get_sr,_set_sr
.globl _get_vbr,_set_vbr
.globl _get_sp,_set_sp
.globl _get_pr,_set_pr

_get_sr:
	stc	sr,r0
	rts
	nop

_set_sr:
	ldc	r4,sr
	rts
	nop

_get_vbr:
	stc	vbr,r0
	rts
	nop

_set_vbr:
	ldc	r4,vbr
	rts
	nop

_get_sp:
	mov	r15,r0
	rts
	nop

_set_sp:
	mov	r4,r15
	rts
	nop

_get_pr:
	sts	pr,r0
	rts
	nop

_set_pr:
	lds	r4,pr
	rts
	nop
