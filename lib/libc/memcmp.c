#include <string.h>
#include <ctype.h>

int memcmp(const void *cs, const void *ct, size_t count)
{
	const unsigned char *su1, *su2;
	signed char res = 0;

	for(su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
		if((res = *su1 - *su2) != 0)
			break;
	return res;
}
