.text
.globl _start
_start:
	j 	0x88004074; // start of the stage2 bootloader
	nop

.data
foo:
#include "boot/mips/final.asminclude"

