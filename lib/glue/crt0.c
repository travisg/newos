extern int __stdio_init();
extern int __stdio_deinit();

extern int main();

int _start()
{

	__stdio_init();
	
	main();
	
	__stdio_deinit();
	return 0;	
}