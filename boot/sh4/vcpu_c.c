#include <vcpu.h>
#include <vcpu_struct.h>
#include <stage2.h>
#include <serial.h>

extern vcpu_struct kernel_struct;

unsigned int vector_base();
unsigned int boot_stack[256] = { 0, };

static int default_vector(unsigned int ex_code, unsigned int pc, unsigned int trap)
{
	dprintf("default_vector: ex_code 0x%x, pc 0x%x, trap 0x%x\r\n", ex_code, pc, trap);
	return 0;
}

unsigned int get_sr();
void set_sr(unsigned int sr);
unsigned int get_vbr();
void set_vbr(unsigned int vbr);
asm("
.globl _get_sr,_set_sr
.globl _get_vbr,_set_vbr

_get_sr:
        stc     sr,r0
        rts
        nop

_set_sr:
        ldc     r4,sr
        rts
        nop

_get_vbr:
        stc     vbr,r0
        rts
        nop

_set_vbr:
        ldc     r4,vbr
        rts
        nop
");

void swap_register_banks(unsigned int new_sr);
asm("
.globl _swap_register_banks
_swap_register_banks:
	ldc	r4,sr
	rts
	nop
");

int vcpu_init(kernel_args *ka)
{
	int i;
	unsigned int sr;
	unsigned int vbr;
	
	dprintf("vcpu_init: entry\r\n");

	for(i=0; i<256; i++) {
		kernel_struct.vt[i].func = &default_vector;
	}
	kernel_struct.kstack = (unsigned int *)((int)boot_stack + sizeof(boot_stack) - 4);

	// set the vbr
	vbr = (unsigned int)&vector_base;
	set_vbr(vbr);
	dprintf("vbr = 0x%x\n", get_vbr());

	// disable exceptions
	sr = get_sr();
	sr |= 0x10000000;
	set_sr(sr);

	if((sr & 0x20000000) != 0) {
		// we're using register bank 1 now
		dprintf("using bank 1, switching register banks\r\n");
		swap_register_banks(sr & 0xdfffffff);
	}

	// enable exceptions
	sr = get_sr();
	sr &= 0xefffffff;	
	set_sr(sr);
	
	ka->vcpu = &kernel_struct;

	return 0;
}

