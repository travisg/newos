#include <string.h>
#include <ctype.h>

int strncmp(const char *cs, const char *ct, size_t count)
{
	signed char __res = 0;

	while(count > 0) {
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
		count--;
	}

	return __res;
}
