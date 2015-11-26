/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>

#include "InputDevice.h"

int InputDevice::Run()
{
    return _kern_thread_resume_thread(_kern_thread_create_thread("InputDevice", &ThreadEntry, this));
}

int InputDevice::ThreadEntry(void *_dev)
{
    InputDevice *dev = (InputDevice *)_dev;
    return dev->_Run();
}

