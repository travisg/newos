#ifndef _PS2_MOUSE_H
#define _PS2_MOUSE_H

const int MAIN_BUTTON = 1;
const int SECONDARY_BUTTON = 2;
const int TERTIARY_BUTTON = 4;

class PS2Mouse {
public:

	PS2Mouse(int xmax, int ymax);
	void GetPos(int *out_x, int *out_y, int *out_buttons);

private:
	int fXPos, fYPos;
	int fXMax, fYMax;
	int fFd;
};


#endif
