#include <stdio.h>
#include <stdlib.h>
#include <sys/syscalls.h>

void testFormatting(FILE* f);
void testLongLine(FILE* f);
void test(FILE* f);

static void testGets(FILE* f)
{
    char buf1[1024];
    char buf2[1024];
    
    fgets(buf1, 1024, f);
    fgets(buf2, 1024, f);
    
    printf("type a line: \"%s\"\r\n", buf1);
    printf("type a line: \"%s\"\r\n", buf2);
}

static void testScanf(FILE* f)
{
	unsigned long l;	
	//argument is ignored for now
	printf("Enter a number:");
	scanf("%u", &l);
	printf("Just some value I chose %u\n", 143);
	printf("You entered: %u\n", l);

}

int main(int argc, char** argv)
{
    char* path;
    FILE* f;

    if(argc > 1)
    {
        f = fopen(argv[1], "r");
        path = argv[1];
    }
    else
    {
        f = stdin;
        path = "stdin";
    }
    printf("A test of \"fscanf\" from file: \"%s\".\r\n\n", path);

    if(f != (FILE*)0)
    {
        testScanf(f);
    }
    else
    {
        printf("File could not be opened.\r\n" );
    }
    
    return 0;
}
