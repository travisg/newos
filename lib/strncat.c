#include <string.h>
#include <ctype.h>

char *strncat(char *dest, const char *src, size_t count)
{
	char *tmp = dest;

	if(count > 0) {
		while(*dest)
			dest++;
		while((*dest++ = *src++)) {
			if (--count == 0) {
				*dest = '\0';
				break;
			}
		}
	}

	return tmp;
}

