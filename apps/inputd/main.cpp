/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/types.h>
#include <win/Event.h>
using os::gui::Event;
#include "InputServer.h"

int main(int argc, char **argv)
{
	InputServer *srv = new InputServer;
	srv->Run();
	delete srv;

	return 0;
}

