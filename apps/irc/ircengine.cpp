/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <newos/errors.h>
#include <socket/socket.h>

#include "ircengine.h"

IRCEngine::IRCEngine()
	:	mReader(NULL),
		mTerm(NULL),
		mInputWindow(NULL),
		mTextWindow(NULL),
		mSocket(-1),
		mInputLinePtr(0)
{
	strncpy(mUser, "user", sizeof(mUser - 1));
	mUser[sizeof(mUser) - 2] = 0;
	strncpy(mIRCName, "name", sizeof(mIRCName - 1));
	mUser[sizeof(mIRCName) - 2] = 0;
	strncpy(mNick, "newos", sizeof(mNick - 1));
	mUser[sizeof(mNick) - 2] = 0;

	mCurrentChannel[0] = 0;

	mSem = sys_sem_create(1, "irc engine lock");
}

IRCEngine::~IRCEngine()
{
	if(mInputWindow)
		delete mInputWindow;
	if(mTextWindow)
		delete mTextWindow;
	if(mReader)
		delete mReader;
	if(mSocket >= 0)
		socket_close(mSocket);
	if(mTerm)
		delete mTerm;

	sys_sem_delete(mSem);
}

void IRCEngine::Lock()
{
	sys_sem_acquire(mSem, 1);
}

void IRCEngine::Unlock()
{
	sys_sem_release(mSem, 1);
}

int IRCEngine::Run()
{
	mTerm = new Term(0);
	mTerm->ClearScreen();

	mTextWindow = new TermWindow(mTerm, 1, 1, 80, 24);
	mInputWindow = new TermWindow(mTerm, 1, 25, 80, 1);

	int err = SignOn();
	if(err < 0)
		return err;

	mReader = new IRCReader(mSocket, this);
	if(!mReader)
		return ERR_NO_MEMORY;

	// starts the reader thread
	mReader->Start();

	// start the keyboard loop
	for(;;) {
		char c;

		err = read(0, &c, sizeof(c));
		if(err < 0)
			break;

		if(err > 0) {
			err = ReceivedKeyboardData(&c, err);
			if(err < 0)
				break;
		}
	}

	Disconnect();

	return 0;
}

int IRCEngine::SignOn()
{
	ssize_t err;

	mSocket = socket_create(SOCK_PROTO_TCP, 0);
	printf("created socket, mSocket %d\n", mSocket);
	if(mSocket < 0)
		return mSocket;

	sockaddr saddr;
	memset(&saddr, 0, sizeof(saddr));
	saddr.addr.len = 4;
	saddr.addr.type = ADDR_TYPE_IP;
	saddr.port = 6667;
	NETADDR_TO_IPV4(saddr.addr) = IPV4_DOTADDR_TO_ADDR(196,40,71,132); // irc.freenode.net
//	NETADDR_TO_IPV4(saddr.addr) = IPV4_DOTADDR_TO_ADDR(63,203,215,73); // newos.org

	err = socket_connect(mSocket, &saddr);
	printf("socket_connect returns %ld\n", err);
	if(err < 0)
		return err;

	char buffer[1024];
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

int IRCEngine::Disconnect()
{
	socket_close(mSocket);
	mSocket = -1;
	return 0;
}

int IRCEngine::SetServer(ipv4_addr serverAddress)
{
	return 0;
}

int IRCEngine::SocketError(int error)
{
	return 0;
}

ssize_t IRCEngine::WriteData(const char *data)
{
	mTextWindow->Write(data, true);

	return write(mSocket, data, strlen(data));
}

int IRCEngine::ReceivedSocketData(char *line, int len)
{
	int i;

	if(len <= 0)
		return 0;

	Lock();

	// parse this line

	// trim the leading spaces
	int pos = 0;
	while(isspace(line[pos]))
		pos++;

	// trim the trailing \n
	for(i = pos; line[i] != 0; i++) {
		if(line[i] == '\n') {
			line[i] = 0;
			break;
		}
	}

	if(line[pos] != ':') {
		Unlock();
		return 0;
	}
	pos++; // eat the ':'
	int start = pos;

	// find the end of the address field
	while(!isspace(line[pos])) {
		if(line[pos] == 0) {
			Unlock();
			return 0;
		}
		pos++;
	}

	// copy out the address field
	char address[256];
	strlcpy(address, &line[start], pos - start + 1);

	// trim some spaces
	while(isspace(line[pos]))
		pos++;

	// look for the command field
	if(strncasecmp(&line[pos], "PRIVMSG ", strlen("PRIVMSG ")) == 0) {
		pos += strlen("PRIVMSG ");

		// eat any spaces leading up to the target field
		while(isspace(line[pos]))
			pos++;
		int target_start = pos;

		// eat the target field
		while(!isspace(line[pos])) {
			if(line[pos] == 0) {
				Unlock();
				return 0;
			}
			pos++;
		}

		char target[256];
		strlcpy(target, &line[target_start], pos - target_start + 1);

		// now each chars up to and including ':'
		while(line[pos] != ':') {
			if(line[pos] == 0) {
				Unlock();
				return 0;
			}
			pos++;
		}
		pos++; // eat the ':'

		// go back and find just the nick in the address field
		for(i = 0; address[i] != 0; i++) {
			if(address[i] == '!') {
				address[i] = 0;
				break;
			}
		}

		// rest of the line is the string
		char outbuf[4096];

		sprintf(outbuf, "%s (%s): %s\n", target, address, &line[pos]);
		mTextWindow->Write(outbuf, true);
	} else {
		// Display the raw line
		mTextWindow->Write(line, true);
	}

	Unlock();
	return 0;
}

int IRCEngine::ReceivedKeyboardData(const char *data, int len)
{
	if(len <= 0)
		return 0;

	Lock();

	for(int i = 0; i < len; i++) {
		if(data[i] == 0x08) { // backspace
			if(mInputLinePtr > 0)
				mInputLinePtr--;
			mInputLine[mInputLinePtr] = 0;
		} else if((data[i] >= ' ' && data[i] < 127) || data[i] == '\n') {
			mInputLine[mInputLinePtr++] = data[i];
			mInputLine[mInputLinePtr] = 0;
			if(mInputLinePtr == sizeof(mInputLine) - 1 || data[i] == '\n') {
				if(ProcessKeyboardInput(mInputLine) < 0)
					return -1;
				mInputLine[0] = 0;
				mInputLinePtr = 0;
			}
		} else {
			// eat it
			continue;
		}
		mInputWindow->Clear();
		mInputWindow->SetCursor(1, 1);
		mInputWindow->Write(mInputLine, true);
	}

	Unlock();

	return 0;
}

int IRCEngine::ProcessKeyboardInput(char *line)
{
	int pos = 0;
	int len = strlen(line);
	char outbuf[4096];

	if(len == 0)
		return 0;

	// trim the leading spaces
	while(isspace(line[pos]))
		pos++;

	if(pos == len)
		return 0;

	// trim the trailing \n
	for(int i = pos; line[i] != 0; i++) {
		if(line[i] == '\n') {
			line[i] = 0;
			len = i;
			break;
		}
	}

	// see if we have a command or just normal text
	if(line[pos] != '/') {
		// we have normal text, write to the current channel
		if(strlen(mCurrentChannel) > 0) {
			sprintf(outbuf, "PRIVMSG %s :%s\n", mCurrentChannel, line);
			WriteData(outbuf);
		}
	} else {
		// leading '/'
		pos++;
		if(strncasecmp(&line[pos], "JOIN ", strlen("JOIN ")) == 0) {
			pos += strlen("JOIN ");
			if(strlen(&line[pos]) > 0) {
				sprintf(outbuf, "JOIN %s\n", &line[pos]);
				WriteData(outbuf);
				strlcpy(mCurrentChannel, &line[pos], sizeof(mCurrentChannel));
			}
		} else if(strncasecmp(&line[pos], "PART ", strlen("PART ")) == 0) {
			pos += strlen("PART ");
			if(strlen(&line[pos]) > 0) {
				sprintf(outbuf, "PART %s\n", &line[pos]);
				WriteData(outbuf);
			}
		} else if(strncasecmp(&line[pos], "NICK ", strlen("NICK ")) == 0) {
			pos += strlen("NICK ");
			if(strlen(&line[pos]) > 0) {
				sprintf(outbuf, "NICK %s\n", &line[pos]);
				WriteData(outbuf);
			}
		} else if(strncasecmp(&line[pos], "ME ", strlen("ME ")) == 0) {
			pos += strlen("ME ");
			if(strlen(&line[pos]) > 0) {
				sprintf(outbuf, "PRIVMSG %s :%cACTION %s%c\n", mCurrentChannel, 0x01, &line[pos], 0x01);
				WriteData(outbuf);
			}
		} else if(strncasecmp(&line[pos], "RAW ", strlen("RAW ")) == 0) {
			pos += strlen("RAW ");
			if(strlen(&line[pos]) > 0) {
				sprintf(outbuf, "%s\n", &line[pos]);
				WriteData(outbuf);
			}
		} else if(strncasecmp(&line[pos], "QUIT", strlen("QUIT")) == 0) {
			WriteData("QUIT\n");
			return -1;
		} else if(strncasecmp(&line[pos], "CLEAR", strlen("CLEAR")) == 0) {
			mTextWindow->Clear();
		}
	}

	return 0;
}

