/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/types.h>
#include <sys/syscalls.h>
#include <unistd.h>
#include <stdio.h>

#include <win/Event.h>
using namespace os::gui;

#include "InputServer.h"
#include "KeyboardDevice.h"
#include "PS2Device.h"

InputServer::InputServer()
{
	mEventPort = _kern_port_create(64, "input_event_port");
	mLock = _kern_sem_create(1, "input_event_lock");
}

InputServer::~InputServer()
{
	_kern_port_delete(mEventPort);
	_kern_sem_delete(mLock);
}

int InputServer::FindDevices()
{
	int fd;

	// for now we'll just cycle through every possible type
	fd = open("/dev/keyboard", 0);
	if(fd >= 0) {
		InputDevice *dev = new KeyboardDevice(fd, this);

		dev->Run();
	}

	fd = open("/dev/ps2mouse", 0);
	if(fd >= 0) {
		InputDevice *dev = new PS2Device(fd, this);

		dev->Run();
	}

	return 0;
}

void InputServer::PostEvent(const Event &event)
{
	_kern_sem_acquire(mLock, 1);

#if 0
	printf("InputServer::PostEvent: what %d ", event.what);
	switch(event.what) {
		case EVT_KEY_DOWN:
		case EVT_KEY_UP:
			printf("key 0x%x mod 0x%x\n", event.key, event.modifiers);
			break;
		case EVT_MOUSE_MOVED:
			printf("x %d y %d mod 0x%x\n", event.x, event.y, event.modifiers);
			break;
		default:
			printf("unknown format\n");
	}
#endif

	_kern_port_write(mEventPort, 0, const_cast<Event *>(&event), sizeof(event));

	_kern_sem_release(mLock, 1);
}

int InputServer::Run()
{
	FindDevices();

	for(;;)
		sleep(1);

	return 0;
}

