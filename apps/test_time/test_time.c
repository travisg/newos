#include <time.h>
#include <stdio.h>

int main(int argc, char **argv)
{
        time_t t;
        time(&t);
		//printf("time_t: %ld\n", t);
        printf("asctime: %s\n", asctime(localtime(&t)));
        return 0;
}

