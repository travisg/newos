/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _INPUTSERVER_H
#define _INPUTSERVER_H

class InputServer {
public:
	InputServer();
	~InputServer();

	int Run();
	void PostEvent(const Event &event);

private:
	port_id WaitForConnection();
	int     FindDevices();

	sem_id  mLock;
	port_id mEventPort;
};


#endif

