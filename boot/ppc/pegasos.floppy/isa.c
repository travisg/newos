#include "isa.h"

#define ISA_REG_PTR(x) ((void *)(0xfe000000 | (x)))

void isa_write8(unsigned short port, unsigned char val)
{
	*(char *)ISA_REG_PTR(port) = val;
}

void isa_write16(unsigned short port, unsigned short val)
{
	*(short *)ISA_REG_PTR(port) = val;
}

void isa_write32(unsigned short port, unsigned int val)
{
	*(int *)ISA_REG_PTR(port) = val;
}

unsigned char isa_read8(unsigned short port)
{
	return *(char *)ISA_REG_PTR(port);
}

unsigned short isa_read16(unsigned short port)
{
	return *(short *)ISA_REG_PTR(port);
}

unsigned int isa_read32(unsigned short port)
{
	return *(int *)ISA_REG_PTR(port);
}
