#include <string.h>
#include <ctype.h>

char *strcpy(char *dest, const char *src)
{
	char *tmp = dest;

	while((*dest++ = *src++) != '\0')
		;
	return tmp;
}

