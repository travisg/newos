/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include "rld_priv.h"

int
RLD_STARTUP(void *args)
{
#if DEBUG_RLD
	_kern_close(0); _kern_open("/dev/dprint", 0); /* stdin   */
	_kern_close(1); _kern_open("/dev/dprint", 0); /* stdout  */
	_kern_close(2); _kern_open("/dev/dprint", 0); /* stderr  */
#endif
	return rldmain(args);
}
