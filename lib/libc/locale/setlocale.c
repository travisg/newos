/* 
** Copyright 2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <locale.h>
#include <string.h>

static char categories[_LC_LAST][32] = {
	"C",
	"C",
	"C",
	"C",
	"C",
	"C",
	"C",
};

char *setlocale(int category, const char *locale)
{
	int i;

	if(category < LC_ALL || category >= _LC_LAST)
		return NULL;

	if(locale) {
		if(LC_ALL) {
			for(i=0; i < _LC_LAST; i++) {
				strlcpy(categories[i], locale, sizeof(categories[i]));
			}
		} else {
			strlcpy(categories[category], locale, sizeof(categories[category]));
		}
		return locale;
	} else {
		return categories[category];
	}
}

