/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _PS2DEVICE_H
#define _PS2DEVICE_H

#include "InputDevice.h"

class PS2Device : public InputDevice {
public:
	PS2Device(int fd, InputServer *srv);
	virtual ~PS2Device();

private:
	virtual int _Run();

	int mFd;
	InputServer *mSrv;
};

#endif

