#include <string.h>
#include <ctype.h>

void *memscan(void *addr, int c, size_t size)
{
	unsigned char *p = (unsigned char *)addr;

	while(size) {
		if(*p == c)
			return (void *)p;
		p++;
		size--;
	}
  	return (void *)p;
}
