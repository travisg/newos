/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/syscalls.h>
#include <stdio.h>
#include <unistd.h>
#include <socket/socket.h>

#include "ircreader.h"

IRCReader::IRCReader(int socket)
	:	mSocket(socket)
{
}

IRCReader::~IRCReader()
{
}

