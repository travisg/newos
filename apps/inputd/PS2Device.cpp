/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/types.h>
#include <unistd.h>
#include <win/Event.h>
using namespace os::gui;

#include "InputServer.h"
#include "PS2Device.h"

const int MAIN_BUTTON = 1;
const int SECONDARY_BUTTON = 2;
const int TERTIARY_BUTTON = 4;

PS2Device::PS2Device(int fd, InputServer *srv)
	:	InputDevice(fd, srv),
		mFd(fd),
		mSrv(srv)
{
}

PS2Device::~PS2Device()
{
}

int PS2Device::_Run()
{
	for(;;) {
		struct mouse_data {
			char status;
			char delta_x;
			char delta_y;
		} data;
		ssize_t len;
		Event event;

		len = read(mFd, &data, sizeof(mouse_data));
		if(len < 0)
			break;

		event.what = EVT_MOUSE_MOVED;
		event.x = data.delta_x;
		event.y = -data.delta_y;

		event.modifiers = 0;
		event.modifiers |= (data.status & 0x01) ? MAIN_BUTTON : 0;
		event.modifiers |= (data.status & 0x02) ? SECONDARY_BUTTON : 0;
		event.modifiers |= (data.status & 0x04) ? TERTIARY_BUTTON : 0;

		mSrv->PostEvent(event);
	}

	return 0;
}

