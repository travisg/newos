#include "asm.h"
#include "serial.h"

#define EXPEVT	0xff000024
#define INTEVT	0xff000028
#define TRA	0xff000020

unsigned int vector_base();
void trap();

void test_interrupt()
{
	unsigned int sr;
	unsigned int vbr;

	dprintf("setting up interrupts\r\n");

	dprintf("sr = 0x%x\r\n", get_sr());
	sr = get_sr();
	sr &= 0xefffffff;
	set_sr(sr);
	dprintf("sr = 0x%x\r\n", get_sr());
	
	dprintf("vbr = 0x%x\r\n", get_vbr());
	vbr = get_vbr();
	vbr = (unsigned int)&vector_base;
	set_vbr(vbr);
	dprintf("vbr = 0x%x\r\n", get_vbr());

	trap();
	trap();

	
	dprintf("done\r\n");
}

void int_handler()
{
	unsigned int exp = *(unsigned int *)EXPEVT & 0xfff;

	serial_puts("int_handler(): entry\r\n");
	dprintf("sr = 0x%x, exp = 0x%x\r\n", get_sr(), exp);
	switch(exp) {
		case 0x80:
		case 0x40:
		case 0x60:
			dprintf("TEA = 0x%x\r\n", *(unsigned int *)0xff00000c);
			dprintf("PC = 0x%x\r\n", get_spc());
			for(;;);
			break;
		case 0x1a0:
			dprintf("illegal slot exception: PC = 0x%x\r\n", get_spc());
		case 0x100:
			for(;;);
		default:
			;
	}
}

