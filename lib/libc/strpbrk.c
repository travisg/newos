#include <string.h>
#include <ctype.h>

char *strpbrk(const char *cs, const char *ct)
{
	const char *sc1;
	const char *sc2;

	for(sc1 = cs; *sc1 != '\0'; ++sc1) {
		for(sc2 = ct; *sc2 != '\0'; ++sc2) {
			if(*sc1 == *sc2)
				return (char *)sc1;
		}
	}
	return NULL;
}
