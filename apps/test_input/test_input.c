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
	char c[10];
	char s[11];
	
	unsigned int u;
	int i;
	int d;
	int x;
	//argument is ignored for now
	printf("Read an unsigned int:");
	scanf("%u", &u);
	printf("Read an integer(undetermined base):");
	scanf("%i", &i);
	printf("Read an integer (base 10):");
	scanf("%d", &d);
	printf("Read an integer (base 16):");
	scanf("%x", &x);
	printf("Read a an char array (10):");
	scanf("%10c", c);
	printf("Read a an string (10):");
	scanf("%10s", s);

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
