#include <string.h>
#include <ctype.h>

void *memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = (char *)dest;
	char *s = (char *)src;

	while(count--)
		*tmp++ = *s++;

	return dest;
}
