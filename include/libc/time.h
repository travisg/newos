/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Copyright 2003, Justin Smith. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__libc_stdio__hh__
#define __newos__libc_stdio__hh__

struct __tm
{
	int tm_sec;    /* seconds*/
	int tm_min;    /* minutes*/
	int tm_hour;   /* hours*/
	int tm_mday;   /* day of the month */
	int tm_mon;    /* months (0 to 11) */
	int tm_year;   /* years */
	int tm_wday;   /* day of week (0 to 6) */
	int tm_yday;   /* day of year (0 to 365) */
	int tm_isdst;  /* Daylight Savings Time */
};

typedef unsigned long size_t;
typedef unsigned long clock_t;
typedef unsigned long time_t;

typedef struct __tm tm;

char *asctime(const struct tm *timeptr);
clock_t clock(void);
char *ctime(const time_t *timer);
double difftime(time_t time1, time_t time2);
struct tm *gmtime(const time_t *timer);
struct tm *localtime(const time_t *timer);
time_t mktime(struct tm *timeptr);
size_t strftime(char *str, size_t maxsize, const char *format, const struct tm *timeptr);
time_t time(time_t *timer);

#endif