void _start(void)
{
	
	*(char *)0x08000100 = 't';
	*(char *)0x08000102 = 's';
	*(char *)0x08000104 = 'e';
	*(char *)0x08000106 = 'T';

	for(;;);

	return;
}
