#include <stdio.h>
#include <stdlib.h>
#include <sys/syscalls.h>

void testFormatting(FILE* f);
void testLongLine(FILE* f);
void test(FILE* f);

void testFormatting(FILE* f)
{
    int* ptr;
    ptr = (int*)malloc(sizeof(int));

    fprintf(f, "char(10): \'%10c\'\r\n", 'c');
    fprintf(f, "string(5): \"%5s\"\r\n", "Justin Smith");
    fprintf(f, "pointer(10): \"%10p\"\r\n", ptr);
    fprintf(f, "so far(20): %n", ptr);
    fprintf(f, "\"%20d\"\r\n", *ptr);
    fprintf(f, "octal(10):21: \"%10o\"\r\n", 21);
    fprintf(f, "hex(5):-21: \"%5x\"\r\n", -21);
    fprintf(f, "signed int(10):-21: \"%10d\"\r\n", -21);
    fprintf(f, "unsigned int(20):-21: \"%20u\"\r\n", -21);

    free(ptr);
}

void testLongLine(FILE* f)
{
    fprintf(f, "To fully test the TTY, one should lower the line buffer size to less than 512.  A very long line: asdlaskd sdflsdkjh dsadlfjh fsdlfajdsfh \
    sdfalasdjkfsdaf sdflsdajkfhsdf sdflasdjfhsdf ds fsdljfhdsaf sdafljksdfhsdlfkjsdfhsd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd \
    sdalkfjsdfh dslfjksdafhsda fsdlafjksdafh sdaflsdafjsdhf asdflsdfjkasdhf dsflasdjkfsdhf sdf sdafjhsdaf sdaflsdjf sdalfjdsfh we flsdjfhsdf sdlafjsdf sd\r\n");
}

static void testPrintf(FILE* f)
{
    testLongLine(f);
    testFormatting(f);
}

int main(int argc, char** argv)
{
    char* path;
    FILE* f;

    if(argc > 1)
    {
        f = fopen(argv[1], "w");
        path = argv[1];
    }
    else
    {
        f = stdout;
        path = "stdout";
    }
    printf("A test of \"fprintf\" to file: \"%s\".\r\n\n", path);

    if(f != (FILE*)0)
    {
        testPrintf(f);
    }
    else
    {
        printf("File could not be opened.\r\n" );
    }

    return 0;
}
