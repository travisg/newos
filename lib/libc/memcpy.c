#include <libc/string.h>
#include <libc/ctype.h>

void *memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = (char *)dest;
	char *s = (char *)src;

	while(count--)
		*tmp++ = *s++;

	return dest;
}
