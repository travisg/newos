#include <boot.h>
#include <stage2.h>
#include <string.h>

#include "asm.h"
#include "serial.h"
#include "mmu.h"

#define BOOTDIR 0x8c000200

int _start()
{
	int i;
	boot_entry *bootdir = (boot_entry *)BOOTDIR;
	unsigned int bootdir_len;
	unsigned int kernel_size;

	serial_init();
	mmu_init();

	serial_puts("Stage 2 loader entry\r\n");

	// look at the bootdir
	{
		bootdir_len = 0;
		for(i=0; i<64; i++) {
			if(bootdir[i].be_type == BE_TYPE_NONE)
				break;
			bootdir_len += bootdir[i].be_size;
		}

		dprintf("bootdir is %d pages long\r\n", bootdir_len);
	}

	// map the kernel text & data
	kernel_size = bootdir[2].be_size;	
	dprintf("kernel is %d pages long\r\n", kernel_size);		

	dprintf("spinning forever...\r\n");
	for(;;);
}
	
