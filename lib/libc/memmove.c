#include <string.h>
#include <ctype.h>

void *memmove(void *dest, const void *src, size_t count)
{
	char *tmp, *s;

	if(dest <= src) {
		tmp = (char *)dest;
		s = (char *)src;
		while(count-- > 0)
			*tmp++ = *s++;
		}
	else {
		tmp = (char *)dest + count;
		s = (char *)src + count;
		while(count-- > 0)
			*--tmp = *--s;
		}

	return dest;
}
