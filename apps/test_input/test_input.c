#include <stdio.h>
#include <stdlib.h>

void testFormatting(FILE* f);
void testLongLine(FILE* f);
void test(FILE* f);

static void testGets(FILE*);

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
	int o;
	int x;
	//argument is ignored for now
	printf("Read an unsigned int:");
	scanf("%u", &u);
	printf("Unsigned Integer value: %u\n", u);
	
	printf("Read an integer(undetermined base):");
	scanf("%i", &i);
	printf("Integer value: %i\n", i);
	
	printf("Read an integer (base 10):");
	scanf("%d", &d);
	printf("Integer value: %i\n", d);

	printf("Read an integer (base 8):");
	scanf("%o", &o);
	printf("Integer value: %i\n", o);
	
	printf("Read an integer (base 16):");
	scanf("%x", &x);
	printf("Integer value: %i\n", x);
	
	printf("Read a char array (10):");
	scanf("%10c", c);
	c[9] = '\0';
	printf("String value: %s\n", c);
	
	printf("Read a string (10):");
	scanf("%10s", s);
	printf("String value: %s\n", s);

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
