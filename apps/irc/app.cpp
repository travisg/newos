/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
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

	mEngine->SetServer(IPV4_DOTADDR_TO_ADDR(64,191,197,245), 6667); // irc.idlenet.net
//	mEngine->SetServer(IPV4_DOTADDR_TO_ADDR(82,96,64,2), 6667); // irc.freenode.net
//	mEngine->SetServer(IPV4_DOTADDR_TO_ADDR(63,203,215,73), 6667); // newos.org

	err = mEngine->Run();

	return 0;
}

int main(int argc, char **argv)
{
	IRCApp app;

	return app.Run(argc, argv);
}

