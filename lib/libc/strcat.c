#include <string.h>
#include <ctype.h>

char *strcat(char *dest, const char *src)
{
	char *tmp = dest;

	while(*dest)
		dest++;
	while((*dest++ = *src++) != '\0')
		;

	return tmp;
}

