#ifndef __ISA_H
#define __ISA_H

void isa_write8(unsigned short port, unsigned char val);
void isa_write16(unsigned short port, unsigned short val);
void isa_write32(unsigned short port, unsigned int val);
unsigned char isa_read8(unsigned short port);
unsigned short isa_read16(unsigned short port);
unsigned int isa_read32(unsigned short port);

#endif
