/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
.text
// void getibats(int bats[8]);
.globl getibats
getibats:
	mfibatu 0,0
	stw     0,0(3)
	mfibatl 0,0
	stwu	0,4(3)
	mfibatu 0,1
	stwu	0,4(3)
	mfibatl 0,1
	stwu	0,4(3)
	mfibatu 0,2
	stwu	0,4(3)
	mfibatl 0,2
	stwu	0,4(3)
	mfibatu 0,3
	stwu	0,4(3)
	mfibatl 0,3
	stwu	0,4(3)
	blr

// void setibats(int bats[8]);
.globl setibats
setibats:
	mfmsr	8
	li		0,0
	mtmsr	0

	lwz		0,0(3)
	mtibatu 0,0
	isync
	lwzu	0,4(3)
	mtibatl 0,0
	isync
	lwzu	0,4(3)
	mtibatu 1,0
	isync
	lwzu	0,4(3)
	mtibatl 1,0
	isync
	lwzu	0,4(3)
	mtibatu 2,0
	isync
	lwzu	0,4(3)
	mtibatl 2,0
	isync
	lwzu	0,4(3)
	mtibatu 3,0
	isync
	lwzu	0,4(3)
	mtibatl 3,0

	isync

	mtmsr	8
	isync
	blr

// void getdbats(int bats[8]);
.globl getdbats
getdbats:
	mfdbatu 0,0
	stw     0,0(3)
	mfdbatl 0,0
	stwu	0,4(3)
	mfdbatu 0,1
	stwu	0,4(3)
	mfdbatl 0,1
	stwu	0,4(3)
	mfdbatu 0,2
	stwu	0,4(3)
	mfdbatl 0,2
	stwu	0,4(3)
	mfdbatu 0,3
	stwu	0,4(3)
	mfdbatl 0,3
	stwu	0,4(3)
	blr

// void setdbats(int bats[8]);
.globl setdbats
setdbats:
	mfmsr	8
	li		0,0
	mtmsr	0

	lwz		0,0(3)
	mtdbatu 0,0
	lwzu	0,4(3)
	mtdbatl 0,0
	lwzu	0,4(3)
	mtdbatu 1,0
	lwzu	0,4(3)
	mtdbatl 1,0
	lwzu	0,4(3)
	mtdbatu 2,0
	lwzu	0,4(3)
	mtdbatl 2,0
	lwzu	0,4(3)
	mtdbatu 3,0
	lwzu	0,4(3)
	mtdbatl 3,0

	mtmsr	8
	sync
	blr

// unsigned int getsdr1();
.globl getsdr1
getsdr1:
	mfsdr1	3
	blr

// void setsdr1(unsigned int sdr);
.globl setsdr1
setsdr1:
	sync
	mtsdr1	3
	sync
	blr

// unsigned int getsr(int sr);
.globl getsr
getsr:
	mfsrin	3,3
	blr

// unsigned int getmsr();
.globl getmsr
getmsr:
	mfmsr 3
	blr

// void setmsr(unsigned int msr);
.globl setmsr
setmsr:
	mtmsr 3
	blr


