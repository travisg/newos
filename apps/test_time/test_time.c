#include <time.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	char c[256];
	time_t t = 253470834000000LL;
	printf("clock_t: %Lu\n", t);
	printf("asctime: %s\n", asctime(localtime(&t)));
	printf("mktime: %Lu\n", mktime(localtime(&t)));
	strftime(c, 256, "\n%%\nabb. day: %a \nday: %A\nabb. month: %b\nmonth: %B\nmday: %d\nhour(24): %H\n""hour(12): %I\nday of year: %j\nmonth: %m", localtime(&t));
	printf("%s\n", c);
	strftime(c, 256, "\nminute: %M\nam/pm: %p\nsecond: %S\nday of week:%w\nweek num(Sun): %U\nweek num(Mon): %W\nyear(100): %y\nyear: %Y", localtime(&t));
	printf("%s\n", c);
	return 0;
}

