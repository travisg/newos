#if !KERNEL
/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Copyright 2003, Justin Smith. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <sys/syscalls.h>
#include <time.h>
#include <string.h>

static void asctime_helper(const struct tm *timeptr, char* buf);

/*
The kernel will eventually have a call like this to return the number of 
microseconds since 0001/01/01 00:00:00
*/
static museconds_t getCurrentSecond()
{
	// The very 1st microsecond was on Jan 1, year 1.
	// Sometime near Dec 12, 2003
	return 63207652980000000L;
}


static char numbers[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
static char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static char buf[26];

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


clock_t clock(void)
{
	return -1;
}


#endif
