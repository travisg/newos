/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#define SRAM_START 0x20000000
#define TEMPSTACK (SRAM_START + 0x1000)
#define RELOCATION_TARGET (TEMPSTACK+0x74) // relocate us to just above the stack in sram
#define RELOCATION_SIZE 0x10000 // should be big enough to get the important data

#define LED_DISPLAY(pos, char) \
	ldr		r11,=(0x08000100 + (3-pos) * 2); \
	ldr		r12,=char; \
	strh	r12,[r11];

#define LED_DISPLAY_REG(pos, reg, byte) \
	ldr		r12,=(0x08000100 + (pos) * 2); \
	ldr		r10,=0xf; \
	and		r11,r10,reg, lsr #(byte * 4); \
	ldr		r10,='0'; \
	add		r11,r11,r10; \
	strh	r11,[r12];

.text
.globl _start
_start:
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	LED_DISPLAY(0, ' ')
	LED_DISPLAY(1, ' ')
	LED_DISPLAY(2, ' ')
	LED_DISPLAY(3, ' ')

	LED_DISPLAY(0, '0')
	
	/* load our new stack */
	ldr		sp,=TEMPSTACK

	/* copy ourself to sram and jump */
	ldr		r1,=0
	ldr		r2,=RELOCATION_TARGET
	ldr		r3,=RELOCATION_SIZE
_loop:
//	LED_DISPLAY_REG(0, r3, 0)
//	LED_DISPLAY_REG(1, r3, 1)
//	LED_DISPLAY_REG(2, r3, 2)
//	LED_DISPLAY_REG(3, r3, 3)
	ldr		r4,[r1], #4
	str		r4,[r2], #4
	sub		r3,r3,#4
	cmp		r3,#0
	bpl		_loop

	LED_DISPLAY(0, '1')

	/* branch */
	ldr		r5,=_decompress_start
	mov		lr,pc
	bx		r5

__spin:
	b		__spin
