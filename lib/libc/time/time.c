#if !KERNEL
/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Copyright 2003, Justin Smith. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <sys/syscalls.h>
#include <time.h>
#include <string.h>

#include <stdio.h>

static void asctime_helper(const struct tm *timeptr, char* buf);

/*
The kernel will eventually have a call like this to return the number of 
microseconds since 0001/01/01 00:00:00

static museconds_t getCurrentMUSecond()
{
	// The very 1st microsecond was on Jan 1, year 1.
	// Sometime near Dec 12, 2003
	return 63207652980000000L;
}
*/

static char numbers[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
static char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static int monthDay[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };


static char buf[26];

static struct tm tm_buf;

/*
Return a pointer to a string of the form: "DDD MMM dd hh:mm:ss YYYY"


DDD {Sun, Mon, Tue, Wed, Thu, Fri, Sat}
MMM	{Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec}
dd	{01-31}
hh	{00-23}
mm 	{00-59}
ss	{00-59}
YYYY the year
*/
char *asctime(const struct tm *timeptr)
{
	asctime_helper(timeptr, buf);
	return buf;
}

static void asctime_helper(const struct tm *timeptr, char* buf)
{
	memcpy(buf, days[timeptr->tm_wday], 3);
	buf[3] = ' ';
	memcpy(buf+4, months[timeptr->tm_mon], 3);
	buf[7] = ' ';
	buf[8] = numbers[timeptr->tm_mday/10];
	buf[9] = numbers[timeptr->tm_mday%10];
	buf[10] = ' ';
	buf[11] = numbers[timeptr->tm_hour/10];
	buf[12] = numbers[timeptr->tm_hour%10];
	buf[13] = ':';
	buf[14] = numbers[timeptr->tm_min/10];
	buf[15] = numbers[timeptr->tm_min%10];
	buf[16] = ':';
	buf[17] = numbers[timeptr->tm_sec/10];
	buf[18] = numbers[timeptr->tm_sec%10];
	buf[19] = ' ';
	buf[20] = numbers[timeptr->tm_year/1000];
	buf[21] = numbers[(timeptr->tm_year/100)%10];
	buf[22] = numbers[(timeptr->tm_year/10)%10];
	buf[23] = numbers[timeptr->tm_year%10];
	buf[24] = '\n';
	buf[25] = '\0';
}


double difftime(time_t time1, time_t time2)
{
	return ((double)(time1 - time2))/1000000.0;
}

struct tm *gmtime(const time_t *timer)
{
	return (struct tm*)0;
}

/*
 Returns the processor clock time used since the beginning of the program). 
 return value / CLOCKS_PER_SEC = number of seconds.
*/
clock_t clock(void)
{
	struct thread_info tinfo;
	thread_id tid = _kern_get_current_thread_id();
	if(tid < 0 || _kern_thread_get_thread_info(tid, &tinfo))
	{
		return (clock_t)-1;
	}
	return (clock_t)(tinfo.user_time + tinfo.kernel_time);
}
/*
static time_t _getSecond(time_t museconds)
{
	return (museconds / 1000000L);
}
*/
static int _getMonthFromYDay(int yDay, int isLeapYear)
{
	int i = 12;
	int month_first;
	do
	{
		i--;
		month_first = monthDay[i] + ((i < 2) ? 0 : isLeapYear);
	}
	while(yDay < month_first);
	return i;
}


static long _getLastDayOfYear(long year)
{
	return 365L * year
	+ ((long)year)/4L
	- ((long)year)/400L;
}


static long _getYear(long day)
{
	long y1 = day / 366;
	
	for(; _getLastDayOfYear(y1) <= day; y1++){}
	
	return y1;	
}

static int _isLeapYear(int year)
{
	if(year % 4 == 0 && year % 400 != 0)
	{
		return 1;
	}
	return 0;
}

static int _getDayOfYear(int year, int month, int dayOfMonth)
{
	return dayOfMonth
		+ monthDay[month] 
		+ ((month > 1) ? _isLeapYear(year): 0)
		- 1;
}

static long _getDay(int year, int month, int dayOfMonth)
{
	return _getDayOfYear(year, month, dayOfMonth)
		+ _getLastDayOfYear(year - 1);
}

static struct tm *localtime_helper(const time_t timer, struct tm *tmb)
{
	long long esec = timer / 1000000L;
	long eday = esec / 86400L;
	long daySec = esec % 86400L;
	long dayMin = daySec / 60;
	
	tmb->tm_year = _getYear(eday);
	tmb->tm_yday = eday - _getLastDayOfYear(tmb->tm_year - 1);
	tmb->tm_wday = (eday + 3) % 7;
	tmb->tm_mon  = _getMonthFromYDay(tmb->tm_yday, _isLeapYear(tmb->tm_year));
	tmb->tm_mday = tmb->tm_yday - monthDay[tmb->tm_mon] + 1;
	tmb->tm_hour = dayMin / 60;
	tmb->tm_min = dayMin % 60;
	tmb->tm_sec = daySec % 60;
	
	return tmb;
}


time_t mktime(struct tm* timeptr)
{
	long day = _getDay(timeptr->tm_year, timeptr->tm_mon, timeptr->tm_mday);
	long long secs =  60 * (60 * timeptr->tm_hour + timeptr->tm_min) 
					  + timeptr->tm_sec 
					  + 86400L * day;
	time_t t = 1000000L * secs;

	printf("days: %ld secs: %lu\n", day, secs);					
	printf("time_t: %Lu\n", t);
	localtime_helper(t, timeptr);
	
	return t;
}


/*
*/
struct tm *localtime(const time_t *timer)
{
	return localtime_helper(*timer, &tm_buf);
}

char *ctime(const time_t *timer)
{
	return asctime(localtime(timer));
}

/*
 Returns the current time in microseconds since 00:00 of Jan 1, 01.
*/
time_t time(time_t *timer)
{
	/* This is not the right syscall. */	
	time_t t = (time_t)_kern_system_time();
	
	if(timer != (time_t*)0)
	{
		*timer = t;
	}
	return t;
}

#endif
