#include "stage2_priv.h"
#include <boot/bootdir.h>
#include <boot/stage2.h>

static kernel_args ka = { 0 };

int _start(char *boot_args, char *monitor);
int _start(char *boot_args, char *monitor)
{

	init_nextmon(monitor);
	dprintf("\nNewOS stage2: args '%s'\n", boot_args);

	probe_memory(&ka);

	dprintf("tc 0x%x\n", get_tc());
	dprintf("urp 0x%x\n", get_urp());
	dprintf("srp 0x%x\n", get_srp());
	
	return 0;
}
