.text
.globl _start
_start:
	j 	_start
	nop

.data
foo:
#include "boot/mips/final.asminclude"

