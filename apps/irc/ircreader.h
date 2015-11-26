#ifndef _IRCREADER_H
#define _IRCREADER_H

class IRCEngine;

class IRCReader {
public:
    IRCReader(int socket, IRCEngine *engine);
    virtual ~IRCReader();

    int Start();

private:
    int Run();
    static int ThreadEntry(void *args);
    int mSocket;
    IRCEngine *mEngine;
    thread_id mReaderThread;

    char mBuf[4096];
    int  mBufPos;
};

#endif

