#include <libc/string.h>
#include <libc/ctype.h>

char *bcopy(const char *src, char *dest, int count)
{
	char *tmp = dest;

	while (count--)
		*tmp++ = *src++;

	return dest;
}
