#include <time.h>
#include <stdio.h>

int main(int argc, char **argv)
{
        time_t t = 253470834000000LL;
		printf("clock_t: %Lu\n", t);
        printf("asctime: %s\n", asctime(localtime(&t)));
		printf("mktime: %Lu\n", mktime(localtime(&t)));
        return 0;
}

