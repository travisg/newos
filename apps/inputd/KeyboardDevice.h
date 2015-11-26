/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KEYBOARDDEVICE_H
#define _KEYBOARDDEVICE_H

#include "InputDevice.h"

class KeyboardDevice : public InputDevice {
public:
    KeyboardDevice(int fd, InputServer *srv);
    virtual ~KeyboardDevice();

private:
    virtual int _Run();

    int mFd;
    InputServer *mSrv;
};

#endif

