#ifndef _SERIAL_MOUSE_H
#define _SERIAL_MOUSE_H

struct mouse_data_packet;

const int MAIN_BUTTON = 1;
const int SECONDARY_BUTTON = 2;

class SerialMouse {
public:

	SerialMouse(int xmax, int ymax);
	void GetPos(int *out_x, int *out_y, int *out_buttons);

private:

	unsigned char SerialRead();

	int fXPos, fYPos;
	int fXMax, fYMax;
};


#endif
