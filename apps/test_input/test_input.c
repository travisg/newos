#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 1024

#define U_VAL 0xffffffe9
#define I_VAL 0xffffffad
#define D_VAL 0x40000000
#define O_VAL 0xfffffff8
#define X_VAL 0xfffffff0
#define S_VAL "45"

static char c_val[] = { '1', '2', '3', '4', '5', (const char)10, '1', '2', '3', (const char)0};

void testFormatting(FILE* f);
void testLongLine(FILE* f);
void test(FILE* f);

static void testScanf( FILE* f, int standardFile)
{
	char u_buf[BUF_SIZE];
	char i_buf[BUF_SIZE];
	char d_buf[BUF_SIZE];
	char o_buf[BUF_SIZE];
	char x_buf[BUF_SIZE];

	char c[10];
	char s[11];

	unsigned int u;
	int i;
	int d;
	int o;
	int x;
	
	printf("\nReading an unsigned int:");
	fgets(u_buf, BUF_SIZE-1, f);
	sscanf(u_buf, "%u", &u);
	
	printf("\nReading an integer(undetermined base):");
	fgets(i_buf, BUF_SIZE-1, f);
	sscanf(i_buf, "%i", &i);

	printf("\nReading an integer (base 10):");
	fgets(d_buf, BUF_SIZE-1, f);
	sscanf(d_buf, "%d", &d);

	printf("\nReading an integer (base 8):");
	fgets(o_buf, BUF_SIZE-1, f);
	sscanf(o_buf, "%o", &o);

	printf("\nReading an integer (base 16):");
	fgets(x_buf, BUF_SIZE-1, f);
	sscanf(x_buf, "%x", &x);

	printf("\nReading a char array (10):");
	fscanf(f,"%9c", c);
	c[10] = '\0';
	
	printf("\nRead a string (10):");
	fscanf(f, "%10s", s);

	printf("\n Hex Integer value: %x\n", u);
	printf(" Hex Integer value: %x\n", i);
	printf(" Hex Integer value: %x\n", d);
	printf(" Hex Integer value: %x\n", o);
	printf(" Hex Integer value: %x\n", x);

	if(standardFile)
	{
		printf("Should be:\n");
		printf(" Hex Integer value: %x\n", U_VAL);
		printf(" Hex Integer value: %x\n", I_VAL);
		printf(" Hex Integer value: %x\n", D_VAL);
		printf(" Hex Integer value: %x\n", O_VAL);
		printf(" Hex Integer value: %x\n", X_VAL);
	}
	
	for(i = 0; i < 10; i++)
	{
		printf("\nc[%d]   =\'%c\'", i, c[i]);
		if(standardFile)
		{
			printf("\nshould ='%c\'", c_val[i]);
		}
	}
	printf("\n");	

	printf("String value:\"%s\"\n",  s);
	if(standardFile)
	{
		printf("Should be   :\"%s\"\n", S_VAL);
	}
}

int main(int argc, char** argv)
{
    int standardText = 0;
	char* path;
    FILE* f;

	
	if(argc > 1)
    {
		if(strstr(argv[1], "stdin") != NULL)
		{
			f = stdin;
			path = "stdin";
		}
		else
		{
			f = fopen(argv[1], "r");
			path = argv[1];
		}
    }
    else
    {
		standardText = 1;
        f = fopen("/boot/etc/test_input.txt", "r");
        path = "/boot/etc/test_input.txt";
    }

    printf("A test of \"fscanf\" from file: \"%s\".\r\n\n", path);

    if(f != (FILE*)0)
    {
        testScanf(f, standardText);
    }
    else
    {
        printf("File could not be opened.\r\n" );
    }

    return 0;
}
