int a;
int b = 1;

int main();

int _start()
{
	char *foo = (char *)0x0;

	*foo = 'Z';

	return main();
}

int main()
{
	asm("int $99");
	return 0;
}


