#include <libc/string.h>
#include <libc/ctype.h>

void *memset(void *s, char c, size_t count)
{
	char *xs = (char *) s;

	while (count--)
		*xs++ = c;

	return s;
}
