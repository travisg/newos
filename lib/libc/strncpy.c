#include <string.h>
#include <ctype.h>

char *strncpy(char *dest, const char *src, size_t count)
{
	char *tmp = dest;

	while(count-- && (*dest++ = *src++) != '\0')
		;

	return tmp;
}

