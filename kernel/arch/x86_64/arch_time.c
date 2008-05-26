/* 
** Copyright 2003-2008, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/arch/time.h>
#include <kernel/arch/x86_64/cpu.h>

static uint64 last_rdtsc;

int arch_time_init(kernel_args *ka)
{
	last_rdtsc = 0;

	return 0;
}

void arch_time_tick(void)
{
	last_rdtsc = x86_64_rdtsc();
}

void setup_system_time(unsigned int cv_factor)
{
#warning implement setup_system_time
}

bigtime_t x86_64_cycles_to_time(uint64 cycles)
{
#warning implement x86_64_cycles_to_time

	return 0;
}

bigtime_t arch_get_time_delta(void)
{
	// XXX race here
	uint64 delta_rdtsc = x86_64_rdtsc() - last_rdtsc;

	return x86_64_cycles_to_time(delta_rdtsc);
}

/* MC146818 RTC code */
static uint8 read_rtc(uint8 reg)
{
	out8(reg, 0x70);
	return in8(0x71);
}

static void write_rtc(uint8 reg, uint8 val)
{
	out8(reg, 0x70);
	out8(val, 0x71);
}

static int bcd2int(uint8 bcd)
{
	return (bcd & 0xf) + ((bcd >> 4) * 10);
}

enum {
	RTC_SEC   = 0,
	RTC_MIN   = 2,
	RTC_HOUR  = 4,
	RTC_DAY   = 7,
	RTC_MONTH = 8,
	RTC_YEAR  = 9,
	RTC_CENTURY = 0x32
};

const bigtime_t usecs_per_day = 86400000000LL;
const int month_days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
const int months[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

static int is_leap_year(int year)
{
	if(year % 4 == 0 && year % 400 != 0) {
		return 1;
	}
	return 0;
}

bigtime_t arch_get_rtc_delta(void)
{
	int oldsec, sec, min, hour;
	int day, month, year, century;
	bigtime_t val;

retry:
	min = bcd2int(read_rtc(RTC_MIN));
	if(min < 0 || min > 59)
		min = 0;
	hour = bcd2int(read_rtc(RTC_HOUR));
	if(hour < 0 || hour > 23)
		hour = 0;
	month = bcd2int(read_rtc(RTC_MONTH));
	if(month < 1 || month > 12)
		month = 1;
	day = bcd2int(read_rtc(RTC_DAY));
	if(day < 1 || day > month_days[month-1])
		day = 1;
	year = bcd2int(read_rtc(RTC_YEAR));
	century = bcd2int(read_rtc(RTC_CENTURY));
	if(century > 0) {
		year += century * 100;
	} else {
		if(year < 80)
			year += 2000;
		else
			year += 1900;
	}

	// keep reading the second counter until it changes
	oldsec = bcd2int(read_rtc(RTC_SEC));
	while((sec = bcd2int(read_rtc(RTC_SEC))) == oldsec)
		;
	if(sec == 0) {
		// we just wrapped around, and potentially changed 
		// all of the other counters, retry everything
		goto retry;
	}

	// convert these values into usecs since Jan 1, 1 AD
	val = (365 * (year - 1) - (year / 4) + (year / 400)) * usecs_per_day;
	val += ((day - 1) + months[month - 1] + ((month > 2) ? is_leap_year(year) : 0)) * usecs_per_day;
	val += (((hour * 60) + min) * 60 + sec) * 1000000;

	dprintf("arch_get_rtc_delta: %d:%d:%d %d/%d/%d = 0x%Lx\n",
		hour, min, sec, month, day, year, val);

	return val;
}

