/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _STRING_H_
#define _STRING_H_

#include <types.h>	/* for size_t */

#ifdef __cplusplus
extern "C" {
#endif

int 	strnicmp(const char *s1, const char *s2, size_t len);
char*	strcpy(char * dest,const char *src);
char*	strncpy(char * dest,const char *src,size_t count);
char*	strcat(char * dest,const char * src);
char* 	strncat(char * dest,const char * src,size_t count);
int 	strcmp(const char * cs,const char * ct);
int 	strncmp(const char * cs,const char * ct,size_t count);
char* 	strchr(const char * s, int c);
char* 	strrchr(const char * s, int c);
size_t 	strlen(const char * s);
size_t 	strnlen(const char * s, size_t count);
size_t 	strspn(const char *s, const char *accept);
char* 	strpbrk(const char * cs,const char * ct);
char* 	strtok(char * s,const char * ct);
void* 	memset(void * s, char c,size_t count);
char* 	bcopy(const char * src, char * dest, int count);
void* 	memcpy(void * to, const void * from, size_t n);
void* 	memmove(void * dest,const void * src, size_t n);
int 	memcmp(const void * cs,const void * ct,size_t count);
void* 	memscan(void * addr, int c, size_t size);
char* 	strstr(const char * s1,const char * s2);
int 	atoi(const char *num);
unsigned int atoui(const char *num);
long atol(const char *num);
unsigned long atoul(const char *num);

#ifdef __cplusplus
}
#endif

#endif

