#ifndef __INT86_H
#define __INT86_H

struct regs {
	unsigned int eax;
	unsigned int ebx;
	unsigned int ecx;
	unsigned int edx;
	unsigned int esi;
	unsigned int edi;
	unsigned short es;
	unsigned short flags;
};

void int86(int interrupt, struct regs *r);

#endif

