#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <sys/syscalls.h>

namespace os {
namespace gui {

class Connection {
public:
	Connection(port_id sendPort, port_id receivePort);
	~Connection();

	int ReadInt32();
	short ReadInt16();
	char ReadInt8();
	void WriteInt32(int);
	void WriteInt16(short);
	void WriteInt8(char);
	void WriteConnectionPort();

	void Write(const void*, int);
	void Flush();
	void EndCommand();
	void Read(void*, int);

private:
	void Receive();

	port_id fSendPort;
	port_id fReceivePort;
	char *fSendBuffer;
	char *fReceiveBuffer;
	int fSendBufferSize;
	int fReceiveBufferSize;
	int fReceiveBufferPos;
};

}
}
#endif

