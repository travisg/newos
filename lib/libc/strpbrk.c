/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <libc/string.h>
#include <libc/ctype.h>

char *strpbrk(const char *cs, const char *ct)
{
	const char *sc1;
	const char *sc2;

	for(sc1 = cs; *sc1 != '\0'; ++sc1) {
		for(sc2 = ct; *sc2 != '\0'; ++sc2) {
			if(*sc1 == *sc2)
				return (char *)sc1;
		}
	}
	return NULL;
}
