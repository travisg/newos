#include <string.h>
#include <ctype.h>

size_t strlen(const char * s)
{
	const char *sc;

	for(sc = s; *sc != '\0'; ++sc)
		;
	return sc - s;
}
