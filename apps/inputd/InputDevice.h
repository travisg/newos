/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _INPUTDEVICE_H
#define _INPUTDEVICE_H

class InputServer;

class InputDevice {
public:
	InputDevice(int fd, InputServer *srv) {};
	virtual ~InputDevice() {};

	int Run();
protected:
	virtual int _Run() = 0;
private:
	static int ThreadEntry(void *dev);
};

#endif

