#ifndef _IRCENGINE_H
#define _IRCENGINE_H

#include "ircreader.h"
#include "term.h"

class IRCEngine {
public:
	IRCEngine();
	~IRCEngine();
	int Run();
	int SignOn();
	int Disconnect();
	int SetServer(ipv4_addr serverAddress);

	int SocketError(int error);
	ssize_t WriteData(const char *data);
	int ReceivedSocketData(char *data, int len);
	int ReceivedKeyboardData(const char *data, int len);
	int ProcessKeyboardInput(char *line);

private:
	void Lock();
	void Unlock();

	IRCReader *mReader;
	Term *mTerm;
	TermWindow *mInputWindow;
	TermWindow *mTextWindow;
	int mSocket;
	char mUser[256];
	char mIRCName[256];
	char mNick[256];
	char mCurrentChannel[256];
	sem_id mSem;

	char mInputLine[1024];
	int  mInputLinePtr;
};

#endif

