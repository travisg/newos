/* 
** Copyright 2001, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <unistd.h>
#include <stdlib.h>
#include <sys/syscalls.h>


/*
 * XXXfreston: this should be a system definition
 */
#define MAXPATHLEN 1024


char *
getcwd(char *buf, size_t len)
{
	int   rc;
	char *_buf;

	_buf= buf?buf:malloc(len);
	rc= sys_getcwd(_buf, len);

	if(rc< 0) {
		if(buf!= _buf) {
			free(_buf);
		}
		_buf= 0;
	}

	return _buf;
}


char *
getwd(char *buf)
{
	return getcwd(buf, MAXPATHLEN);
}
