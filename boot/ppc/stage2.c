/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include "stage2_text.h"

void _start()
{
	s2_text_init();

	s2_printf("Welcome to the stage2 bootloader!\n");
	s2_printf("Next line\n");

#if 1
	{
		int i;
		for(i=0; i<1024; i++) {
			s2_printf("line %d\n", i);
		}
	}
#endif
	for(;;);
}



