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

