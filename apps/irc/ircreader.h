#ifndef _IRCREADER_H
#define _IRCREADER_H

class IRCReader {
	public:
		IRCReader(int socket);
		virtual ~IRCReader();

	private:
		int mSocket;
};

#endif

