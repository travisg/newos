#include "serial.h"
#include "defs.h"
#include <printf.h>

int dprintf(const char *fmt, ...)
{
	int ret = 0; 
	va_list args;
	char temp[128];

	va_start(args, fmt);
	ret = vsprintf(temp, fmt, args);
	va_end(args);

	serial_puts(temp);
	return ret;
}

/* Initialize the SCIF port; baud_rate must be at least 9600 and
   no more than 57600. 115200 does NOT work for most PCs. */
void serial_init() {
	volatile u2 *scif16 = (u2*)0xffe80000;
	volatile u1 *scif8 = (u1*)0xffe80000;
	int x;
	
	/* Disable interrupts, transmit/receive, and use internal clock */
	scif16[8/2] = 0;
	
	/* 8N1, use P0 clock */
	scif16[0] = 0;
	
	/* Set baudrate, N = P0/(32*B)-1 */
//	scif8[4] = (50000000 / (32 * baud_rate)) - 1;

//	scif8[4] = 80; // 19200
//	scif8[4] = 40; // 38400
//	scif8[4] = 26; // 57600
	scif8[4] = 13; // 115200
	
	
	/* Reset FIFOs, enable hardware flow control */	
	scif16[24/2] = 4; //12;


	for(x = 0; x < 100000; x++);
	scif16[24/2] = 0; //8;
	
	/* Disable manual pin control */
	scif16[32/2] = 0;
	
	/* Clear status */
	scif16[16/2] = 0x60;
	scif16[36/2] = 0;
	
	/* Enable transmit/receive */
	scif16[8/2] = 0x30;
	
	for(x = 0; x < 100000; x++);
}

/* Write one char to the serial port (call serial_flush()!)*/
void serial_putc(int c) {
	volatile u2 *ack = (u2*)0xffe80010;
	volatile u1 *fifo = (u1*)0xffe8000c;
	
	/* Wait until the transmit buffer has space */
	while (!(*ack & 0x20))
		;
	
	/* Send the char */
	*fifo = c;
	
	/* Clear status */
	*ack &= 0x9f;
}

/* Flush all FIFO'd bytes out of the serial port buffer */
void serial_flush() {
	volatile u2 *ack = (u2*)0xffe80010;

	*ack &= 0xbf;
	while (!(*ack & 0x40))
		;
	*ack &= 0xbf;
}

/* Send an entire buffer */
void serial_write(u1 *data, int len) {
	while (len-- > 0){
		serial_putc(*data++);
	}
	serial_flush();
}

/* Send a string (null-terminated) */
void serial_puts(char *str) {
	while(*str){
		serial_putc(*str++);
	}
	serial_flush();
}

/* Read one char from the serial port (-1 if nothing to read) */
int serial_read() {
	volatile u2 *status = (u2*)0xffe8001c;
	volatile u2 *ack = (u2*)0xffe80010;
	volatile u1 *fifo = (u1*)0xffe80014;
	int c;
	
	/* Check input FIFO */
	while ((*status & 0x1f) == 0);
	
	/* Get the input char */
	c = *fifo;
	
	/* Ack */
	*ack &= 0x6d;

	return c;
}
void serial_put(u1 *buffer, int count)
{
	u1 x;

	serial_putc(ATTN);
	serial_putc(ATTN);

	x = count & 255;
	if(x == ATTN) {
		serial_putc(SPECIAL);
		serial_putc(ENCODE_ATTN);
	} else if(x == SPECIAL) {
		serial_putc(SPECIAL);
		serial_putc(ENCODE_SPECIAL);
	} else {
		serial_putc(x);
	}
	
	x = count >> 8;
	if(x == ATTN) {
		serial_putc(SPECIAL);
		serial_putc(ENCODE_ATTN);
	} else if(x == SPECIAL) {
		serial_putc(SPECIAL);
		serial_putc(ENCODE_SPECIAL);
	} else {
		serial_putc(x);
	}

	while(count > 0){
		if(*buffer == ATTN) {
			serial_putc(SPECIAL);
			serial_putc(ENCODE_ATTN);
		} else if(*buffer == SPECIAL) {
			serial_putc(SPECIAL);
			serial_putc(ENCODE_SPECIAL);
		} else {
			serial_putc(*buffer);
		}
		buffer++;
		count--;
	}

	serial_flush();
}

int serial_get(u1 *buf)
{
	volatile u2 *status = (u2*)0xffe8001c;
	volatile u2 *ack = (u2*)0xffe80010;
	volatile u1 *fifo = (u1*)0xffe80014;
	
	int c;
	int state;
	int escape;
	int count;
	int l;
	
	state = 3;
	
 restart:
	escape = 0;
	l = 0;
	
	for(;;){
			/* Check input FIFO */
		while ((*status & 0x1f) == 0);
		c = *fifo; /* Get the input char */
		*ack &= 0x6d; /* Ack */

		if(c == ATTN) {
			state = 0;
			goto restart;
		}
		
		if(c == SPECIAL) {
			state |= 8;
			continue;
		}

	restate:
		switch(state){
		case 0:
			count = c;
			state = 1;
			break;
			
		case 1:
			count |= (c << 8);
			if((count > 8192) || (count < 1)){
				state = 3;
			} else {
				state = 2;
			}
			break;

		case 2:
			buf[l] = c;
			l++;
			if(l == count) return count;
			
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
				/* wait for restart */
			break;
			
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			switch(c){
			case ENCODE_ATTN:
				c = ATTN;
				break;
			case ENCODE_SPECIAL:
				c = SPECIAL;
				break;
			}
			state &= 7;
			goto restate;
		}
	}
	
	return c;


}

