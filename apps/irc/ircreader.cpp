/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <unistd.h>
#include <socket/socket.h>

#include "ircreader.h"
#include "ircengine.h"

IRCReader::IRCReader(int socket, IRCEngine *engine)
	:	mSocket(socket),
		mEngine(engine),
		mReaderThread(-1),
		mBufPos(0)
{
}

IRCReader::~IRCReader()
{
	// forgive me father...
	if(mReaderThread >= 0)
		sys_thread_kill_thread(mReaderThread);
}

int IRCReader::Start()
{
	mReaderThread = sys_thread_create_thread("irc reader", &ThreadEntry, (void *)this);
	if(mReaderThread < 0)
		return mReaderThread;

	sys_thread_resume_thread(mReaderThread);

	return 0;
}

int IRCReader::Run()
{
	for(;;) {
		char buf[1024];
		ssize_t bytes_received;

		bytes_received = read(mSocket, buf, sizeof(buf));
		if(bytes_received < 0) {
			mEngine->SocketError(bytes_received);
			break;
		}

		// add this data to the end of the line buffer we have
		for(int i = 0; i < bytes_received; i++) {
			mBuf[mBufPos++] = buf[i];
			if(mBufPos == sizeof(mBuf) - 1 || buf[i] == '\n') {
				mBuf[mBufPos] = 0;
				mEngine->ReceivedSocketData(mBuf, mBufPos);
				mBufPos = 0;
			}
		}
	}
	return 0;
}

int IRCReader::ThreadEntry(void *args)
{
	IRCReader *reader = (IRCReader *)args;

	return reader->Run();
}


