#ifndef _EVENT_H
#define _EVENT_H

const int EVT_PAINT = 1;
const int EVT_QUIT = 2;
const int EVT_MOUSE_DOWN = 3;
const int EVT_MOUSE_UP = 4;
const int EVT_MOUSE_MOVED = 5;
const int EVT_MOUSE_ENTER = 6;
const int EVT_MOUSE_LEAVE = 7;

struct Event {
	int what;
	int target;

	int x, y;
};

#endif

