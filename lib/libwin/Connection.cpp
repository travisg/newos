#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscalls.h>
#include "Connection.h"

namespace os {
namespace gui {

const int kSendBufferSize = 0x1000;
const int kMaxCommandSize = 270;
const int kReceiveBufferSize = 1024;

Connection::Connection(port_id sendPort, port_id receivePort)
    :   fSendPort(sendPort),
        fReceivePort(receivePort),
        fSendBufferSize(0),
        fReceiveBufferSize(0),
        fReceiveBufferPos(0)
{
    fSendBuffer = (char*) malloc(kSendBufferSize);
    fReceiveBuffer = (char*) malloc(kReceiveBufferSize);
}

Connection::~Connection()
{
    free(fSendBuffer);
    free(fReceiveBuffer);
//  port_destroy(fReceivePort);
}

void Connection::Write(const void *data, int size)
{
    int sizeWritten = 0;
    while (sizeWritten < size) {
        int sizeToWrite = size - sizeWritten;
        if (fSendBufferSize + sizeToWrite > kSendBufferSize)
            sizeToWrite = kSendBufferSize - fSendBufferSize;

        if (sizeToWrite == 0) {
            Flush();
            continue;
        }

        memcpy((void*) (fSendBuffer + fSendBufferSize), (const void*) ((const char*) data
                + sizeWritten), sizeToWrite);
        fSendBufferSize += sizeToWrite;
        sizeWritten += sizeToWrite;
    }
}

void Connection::Read(void *data, int size)
{
    int sizeRead = 0;
    while (sizeRead < size) {
        int sizeToRead = size - sizeRead;

        if (fReceiveBufferSize - fReceiveBufferPos < sizeToRead)
            sizeToRead = fReceiveBufferSize - fReceiveBufferPos;

        if (sizeToRead == 0) {
            Receive();
            continue;
        }

        memcpy((void*) ((char*) data + sizeRead),
               (void*) (fReceiveBuffer + fReceiveBufferPos), sizeToRead);
        fReceiveBufferPos += sizeToRead;
        sizeRead += sizeToRead;
    }
}

void Connection::EndCommand()
{
    if (fSendBufferSize > kSendBufferSize - kMaxCommandSize)
        Flush();
}

void Connection::Flush()
{
    printf("Connection::Flush: port %d, size %d\n", fSendPort, fSendBufferSize);
    _kern_port_write(fSendPort, 0, fSendBuffer, fSendBufferSize);

    fSendBufferSize = 0;
}

void Connection::Receive()
{
    int ignore;

    fReceiveBufferSize = _kern_port_read(fReceivePort, &ignore, fReceiveBuffer, kReceiveBufferSize);
    fReceiveBufferPos = 0;
}

int Connection::ReadInt32()
{
    int outval;
    Read(&outval, 4);
    return outval;
}

short Connection::ReadInt16()
{
    short outval;
    Read(&outval, 2);
    return outval;
}

char Connection::ReadInt8()
{
    char outval;
    Read(&outval, 1);
    return outval;
}

void Connection::WriteConnectionPort()
{
    Write(&fReceivePort, sizeof(fReceivePort));
}

void Connection::WriteInt32(int i)
{
    Write(&i, 4);
}

void Connection::WriteInt16(short s)
{
    Write(&s, 2);
}

void Connection::WriteInt8(char c)
{
    Write(&c, 1);
}

}
}

