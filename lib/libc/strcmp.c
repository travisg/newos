#include <libc/string.h>
#include <libc/ctype.h>

int strcmp(const char *cs, const char *ct)
{
	signed char __res;

	while(1) {
		if((__res = *cs - *ct++) != 0 || !*cs++)
			break;
	}

	return __res;
}
