/*
** Copyright 2001-2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
.text

/* void switch_stacks_and_call(unsigned int new_stack, void *func, int arg0, int arg1) */
.globl switch_stacks_and_call
switch_stacks_and_call:
	mr			r1, r3	/* load the stack pointer */
	mtlr		r4
	mr			r3, r5
	mr			r4, r6
	blr

/* void getibats(int bats[8]); */
.globl getibats
getibats:
	mfibatu 	r0,0
	stw     	r0,0(r3)
	mfibatl 	r0,0
	stwu		r0,4(r3)
	mfibatu 	r0,1
	stwu		r0,4(r3)
	mfibatl 	r0,1
	stwu		r0,4(r3)
	mfibatu 	r0,2
	stwu		r0,4(r3)
	mfibatl 	r0,2
	stwu		r0,4(r3)
	mfibatu 	r0,3
	stwu		r0,4(r3)
	mfibatl 	r0,3
	stwu		r0,4(r3)
	blr

// void setibats(int bats[8]);
.globl setibats
setibats:
	mfmsr		r8
	li			r0,0
	mtmsr		r0

	lwz			r0,0(r3)
	mtibatu 	0,r0
	isync
	lwzu		r0,4(r3)
	mtibatl 	0,r0
	isync
	lwzu		r0,4(r3)
	mtibatu 	1,r0
	isync
	lwzu		r0,4(r3)
	mtibatl 	1,r0
	isync
	lwzu		r0,4(r3)
	mtibatu 	2,r0
	isync
	lwzu		r0,4(r3)
	mtibatl 	2,r0
	isync
	lwzu		r0,4(r3)
	mtibatu 	3,r0
	isync
	lwzu		r0,4(r3)
	mtibatl 	3,r0

	isync

	mtmsr		r8
	isync
	blr

// void getdbats(int bats[8]);
.globl getdbats
getdbats:
	mfdbatu 	r0,0
	stw     	r0,0(r3)
	mfdbatl 	r0,0
	stwu		r0,4(r3)
	mfdbatu 	r0,1
	stwu		r0,4(r3)
	mfdbatl 	r0,1
	stwu		r0,4(r3)
	mfdbatu 	r0,2
	stwu		r0,4(r3)
	mfdbatl 	r0,2
	stwu		r0,4(r3)
	mfdbatu		r0,3
	stwu		r0,4(r3)
	mfdbatl 	r0,3
	stwu		r0,4(r3)
	blr

// void setdbats(int bats[8]);
.globl setdbats
setdbats:
	mfmsr		r8
	li			r0,0
	mtmsr		r0

	lwz			r0,0(r3)
	mtdbatu 	0,r0
	lwzu		r0,4(r3)
	mtdbatl 	0,r0
	lwzu		r0,4(r3)
	mtdbatu 	1,r0
	lwzu		r0,4(r3)
	mtdbatl 	1,r0
	lwzu		r0,4(r3)
	mtdbatu 	2,r0
	lwzu		r0,4(r3)
	mtdbatl 	2,r0
	lwzu		r0,4(r3)
	mtdbatu 	3,r0
	lwzu		r0,4(r3)
	mtdbatl 	3,r0

	mtmsr		r8
	sync
	blr

// unsigned int getsdr1();
.globl getsdr1
getsdr1:
	mfsdr1		r3
	blr

// void setsdr1(unsigned int sdr);
.globl setsdr1
setsdr1:
	sync
	mtsdr1		r3
	sync
	blr

// unsigned int getsr(unsigned int va);
.globl getsr
getsr:
	mfsrin		r3,r3
	sync
	blr

// void setsr(unsigned int va, unsigned int val);
.globl setsr
setsr:
	mtsrin		r4,r3
	sync
	blr

// unsigned int getmsr();
.globl getmsr
getmsr:
	mfmsr 		r3
	blr

// void setmsr(unsigned int msr);
.globl setmsr
setmsr:
	mtmsr 		r3
	blr

.globl system_reset_exception_entry
system_reset_exception_entry:
	bla		system_reset_exception
	rfi
.globl system_reset_exception_entry_end
system_reset_exception_entry_end:

.globl machine_check_exception_entry
machine_check_exception_entry:
	bla		machine_check_exception
	rfi
.globl machine_check_exception_entry_end
machine_check_exception_entry_end:

.globl dsi_exception_entry
dsi_exception_entry:
	bla		dsi_exception
	rfi
.globl dsi_exception_entry_end
dsi_exception_entry_end:

.globl isi_exception_entry
isi_exception_entry:
	bla		isi_exception
	rfi
.globl isi_exception_entry_end
isi_exception_entry_end:

.globl external_interrupt_entry
external_interrupt_entry:
	bla		external_interrupt
	rfi
.globl external_interrupt_entry_end
external_interrupt_entry_end:

.globl alignment_exception_entry
alignment_exception_entry:
	bla		alignment_exception
	rfi
.globl alignment_exception_entry_end
alignment_exception_entry_end:

.globl program_exception_entry
program_exception_entry:
	bla		program_exception
	rfi
.globl program_exception_entry_end
program_exception_entry_end:

.globl decrementer_exception_entry
decrementer_exception_entry:
	bla		decrementer_exception
	rfi
.globl decrementer_exception_entry_end
decrementer_exception_entry_end:

.globl system_call_exception_entry
system_call_exception_entry:
	mfmsr	r0
	ori		r0,r0,0x3030
	sync
	mtmsr	r0
	isync

	bla		system_call_exception
	rfi
.globl system_call_exception_entry_end
system_call_exception_entry_end:

.globl trace_exception_entry
trace_exception_entry:
	bla		trace_exception
	rfi
.globl trace_exception_entry_end
trace_exception_entry_end:

.globl floating_point_assist_exception_entry
floating_point_assist_exception_entry:
	bla		floating_point_assist_exception
	rfi
.globl floating_point_assist_exception_entry_end
floating_point_assist_exception_entry_end:

.globl g3_exception_entry
g3_exception_entry:
	bla		g3_exception_entry_exception
	rfi
.globl g3_exception_entry_end
g3_exception_entry_end:

