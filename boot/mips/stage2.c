
asm("
.globl _start
_start:
	j	_start
	nop

");
/*
void _start()
{
	for(;;);
}
*/

