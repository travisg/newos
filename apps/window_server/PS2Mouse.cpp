#include "PS2Mouse.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscalls.h>

struct mouse_data {
	char status;
	char delta_x;
	char delta_y;
}; // mouse_data

PS2Mouse::PS2Mouse(int xmax, int ymax)
	:	fXPos(xmax / 2),
		fYPos(ymax / 2),
		fXMax(xmax),
		fYMax(ymax)
{
	fFd = open("/dev/ps2mouse", 0);
	if(fFd < 0)
		return;

}

void PS2Mouse::GetPos(int *out_x, int *out_y, int *out_buttons)
{
	mouse_data data;
	int err;

retry:
	err = read(fFd, &data, sizeof(mouse_data));
	if(err < 0) {
		sys_snooze(1000000);
		goto retry;
	}

	fXPos += data.delta_x;
	if(fXPos < 0)
		fXPos = 0;
	if(fXPos > fXMax)
		fXPos = fXMax;
	*out_x = fXPos;

	fYPos += data.delta_y;
	if(fYPos < 0)
		fYPos = 0;
	if(fYPos > fYMax)
		fYPos = fYMax;
	*out_y = fYMax - fYPos;

	*out_buttons = 0;
	*out_buttons |= (data.status & 0x80) ? MAIN_BUTTON : 0;
	*out_buttons |= (data.status & 0x40) ? SECONDARY_BUTTON : 0;
	*out_buttons |= (data.status & 0x20) ? TERTIARY_BUTTON : 0;

	return;
}

