#include <string.h>
#include <ctype.h>

char *___strtok = NULL;

int strnicmp(const char *s1, const char *s2, size_t len)
{
	unsigned char c1 = '\0';
	unsigned char c2 = '\0';

	if(len > 0) {
		do {
			c1 = *s1; c2 = *s2;
			s1++; s2++;
			if(!c1)
				break;
			if(!c2)
				break;
			if(c1 == c2)
				continue;
			c1 = tolower(c1);
			c2 = tolower(c2);
			if (c1 != c2)
				break;
		} while(--len);
	}
	return (int)c1 - (int)c2;
}


char *strcpy(char *dest, const char *src)
{
	char *tmp = dest;

	while((*dest++ = *src++) != '\0')
		;
	return tmp;
}

char *strncpy(char *dest, const char *src, size_t count)
{
	char *tmp = dest;

	while(count-- && (*dest++ = *src++) != '\0')
		;

	return tmp;
}

char *strcat(char *dest, const char *src)
{
	char *tmp = dest;

	while(*dest)
		dest++;
	while((*dest++ = *src++) != '\0')
		;

	return tmp;
}

char *strncat(char *dest, const char *src, size_t count)
{
	char *tmp = dest;

	if(count > 0) {
		while(*dest)
			dest++;
		while((*dest++ = *src++)) {
			if (--count == 0) {
				*dest = '\0';
				break;
			}
		}
	}

	return tmp;
}

int strcmp(const char *cs, const char *ct)
{
	signed char __res;

	while(1) {
		if((__res = *cs - *ct++) != 0 || !*cs++)
			break;
	}

	return __res;
}

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

char *strchr(const char *s, int c)
{
	for(; *s != (char) c; ++s)
		if (*s == '\0')
			return NULL;
	return (char *) s;
}

char *strrchr(const char *s, int c)
{
       const char *p = s + strlen(s);
       do {
           if (*p == (char)c)
               return (char *)p;
       } while (--p >= s);
       return NULL;
}

size_t strlen(const char * s)
{
	const char *sc;

	for(sc = s; *sc != '\0'; ++sc)
		;
	return sc - s;
}

size_t strnlen(const char * s, size_t count)
{
	const char *sc;

	for(sc = s; count-- && *sc != '\0'; ++sc)
		;
	return sc - s;
}

size_t strspn(const char *s, const char *accept)
{
	const char *p;
	const char *a;
	size_t count = 0;

	for(p = s; *p != '\0'; ++p) {
		for(a = accept; *a != '\0'; ++a) {
			if(*p == *a)
				break;
		}
		if(*a == '\0')
			return count;
		++count;
	}

	return count;
}

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

char *strtok(char *s, const char *ct)
{
	char *sbegin, *send;

	sbegin  = s ? s : ___strtok;
	if (!sbegin) {
		return NULL;
	}
	sbegin += strspn(sbegin,ct);
	if (*sbegin == '\0') {
		___strtok = NULL;
		return( NULL );
	}
	send = strpbrk( sbegin, ct);
	if (send && *send != '\0')
		*send++ = '\0';
	___strtok = send;
	return (sbegin);
}

void *memset(void *s, char c, size_t count)
{
	char *xs = (char *) s;

	while (count--)
		*xs++ = c;

	return s;
}

char *bcopy(const char *src, char *dest, int count)
{
	char *tmp = dest;

	while (count--)
		*tmp++ = *src++;

	return dest;
}

void *memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = (char *)dest;
	char *s = (char *)src;

	while(count--)
		*tmp++ = *s++;

	return dest;
}

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

int memcmp(const void *cs, const void *ct, size_t count)
{
	const unsigned char *su1, *su2;
	signed char res = 0;

	for(su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
		if((res = *su1 - *su2) != 0)
			break;
	return res;
}

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

char *strstr(const char *s1, const char *s2)
{
	int l1, l2;

	l2 = strlen(s2);
	if (!l2)
		return (char *)s1;
	l1 = strlen(s1);
	while(l1 >= l2) {
		l1--;
		if (!memcmp(s1,s2,l2))
			return (char *)s1;
		s1++;
	}
	return NULL;
}

static int hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
		
	return 0;
}

int atoi(const char *num)
{
	int value = 0;
	if (num[0] == '0' && num[1] == 'x') {
		// hex
		num += 2;
		while (*num && isxdigit(*num))
			value = value * 16 + hexval(*num++);
	} else {
		// decimal
		while (*num && isdigit(*num))
			value = value * 10 + *num++  - '0';
	}

	return value;
}

