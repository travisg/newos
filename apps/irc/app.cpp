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

#include "ircreader.h"

class IRC {
	public:
		IRC();
		~IRC();
		int Run(int argc, char **argv);
		int SignOn();

	private:
		IRCReader *mReader;
		int mSocket;
		char mUser[256];
		char mIRCName[256];
		char mNick[256];
};

IRC::IRC()
	:	mReader(NULL),
		mSocket(-1)
{
	strncpy(mUser, "user", sizeof(mUser - 1));
	mUser[sizeof(mUser) - 2] = 0;
	strncpy(mIRCName, "name", sizeof(mIRCName - 1));
	mUser[sizeof(mIRCName) - 2] = 0;
	strncpy(mNick, "newos", sizeof(mNick - 1));
	mUser[sizeof(mNick) - 2] = 0;
}

IRC::~IRC()
{
	if(mSocket >= 0)
		socket_close(mSocket);
}

int IRC::Run(int argc, char **argv)
{
	int err;
	sockaddr addr;

	mSocket = socket_create(SOCK_PROTO_TCP, 0);
	printf("created socket, mSocket %d\n", mSocket);
	if(mSocket < 0)
		return 0;

	memset(&addr, 0, sizeof(addr));
	addr.addr.len = 4;
	addr.addr.type = ADDR_TYPE_IP;
	addr.port = 6667;
	NETADDR_TO_IPV4(addr.addr) = IPV4_DOTADDR_TO_ADDR(209,131,227,242); // irc.openprojects.net

	err = socket_connect(mSocket, &addr);
	printf("socket_connect returns %d\n", err);
	if(err < 0)
		return err;

	if(SignOn() < 0)
		return ERR_GENERAL;

	return 0;
}

int IRC::SignOn()
{
	char buffer[1024];
	ssize_t err;

	sprintf(buffer, "USER %s xxx xxx :%s\n", mUser, mIRCName);
	err = socket_write(mSocket, buffer, strlen(buffer));
	if(err < (ssize_t)strlen(buffer))
		return ERR_GENERAL;

	sprintf(buffer, "NICK %s\n", mNick);
	err = socket_write(mSocket, buffer, strlen(buffer));
	if(err < (ssize_t)strlen(buffer))
		return ERR_GENERAL;

	return NO_ERROR;
}

int main(int argc, char **argv)
{
	IRC app;

	return app.Run(argc, argv);
}

