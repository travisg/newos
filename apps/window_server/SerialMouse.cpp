#include <i386/io.h>
#include <blt/syscall.h>
#include <stdio.h>
#include "SerialMouse.h"
#include "util.h"

#define POLL 1

const int kBaudRate = 1200;
const int kPortBase = 0x3f8;

SerialMouse::SerialMouse(int xmax, int ymax)
	:	fXPos(xmax / 2),
		fYPos(ymax / 2),
		fXMax(xmax),
		fYMax(ymax)
{
	unsigned short divisor = 115200 / kBaudRate;

	outb(0, kPortBase + 1);					// No interrupts for now
	outb(0, kPortBase + 4);					// clear DTR and RTS
		
	outb(0x80, kPortBase + 3);				// load divisor latch
	outb((divisor & 0xff), kPortBase);		// divisor LSB 
	outb((divisor >> 8), kPortBase + 1);	// divisor MSB 
	outb(2, kPortBase + 3);					// clear DLAB, set up for 7N1

	outb(3, kPortBase + 4);					// Say hello.  Raise DTR and RTS
	if (SerialRead() == 'M')
		printf("Detected MS serial mouse\n");
}


// The data is sent in 3 byte packets in following format:
//         D7      D6      D5      D4      D3      D2      D1      D0 
// 1.      X       1       LB      RB      Y7      Y6      X7      X6 
// 2.      X       0       X5      X4      X3      X2      X1      X0      
// 3.      X       0       Y5      Y4      Y3      Y2      Y1      Y0 
void SerialMouse::GetPos(int *out_x, int *out_y, int *out_buttons)
{
	char b1;
	
	// loop until we get to the beginning of a packet
	while (true) {
		b1 = SerialRead();
		if ((b1 & (1 << 6)) != 0)
			break;		// beginning of packet
	}
	
	char b2 = SerialRead();
	char b3 = SerialRead();
	
	fXPos += (int)(char)((b1 << 6) | (b2 & 0x3f));
	fYPos += (int)(char)(((b1 << 4) & 0xc0) | (b3 & 0x3f));

	fXPos = min(fXPos, fXMax);
	fXPos = max(fXPos, 0);
	fYPos = min(fYPos, fYMax);
	fYPos = max(fYPos, 0);

	*out_buttons = (b1 >> 4) & 3;
	*out_x = fXPos;
	*out_y = fYPos;
}

unsigned char SerialMouse::SerialRead()
{
	while (true) {
#if POLL
		while ((inb(kPortBase + 5) & 1) == 0)
			;
#else
		os_handle_irq(4);
		outb(1, kPortBase + 1);		// Interrupt on recieve ready
		os_sleep_irq();
		int id = inb(kPortBase + 2);
		if ((id & 1) == 0 || (id >> 1) != 2)
			continue;				// Not for me
#endif

		break;
	}
					
	return inb(kPortBase);
}

