/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <newos/errors.h>
#include <socket/socket.h>

#include "ircengine.h"

class IRCApp {
	public:
		IRCApp();
		~IRCApp();
		int Run(int argc, char **argv);

	private:
		IRCEngine *mEngine;
};

IRCApp::IRCApp()
	:	mEngine(NULL)
{
}

IRCApp::~IRCApp()
{
	if(mEngine) {
		mEngine->Disconnect();
		delete mEngine;
	}
}

int IRCApp::Run(int argc, char **argv)
{
	mEngine = new IRCEngine();
	if(!mEngine)
		return ERR_NO_MEMORY;

	int err;

	mEngine->SetServer(IPV4_DOTADDR_TO_ADDR(196,40,71,132));

	err = mEngine->Run();

	return 0;
}

int main(int argc, char **argv)
{
	IRCApp app;

	return app.Run(argc, argv);
}

