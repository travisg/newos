#include <kernel.h>
#include <stage2.h>
#include <dev.h>
#include <vfs.h>
#include <debug.h>

#include <fs/devfs.h>

int dev_init(kernel_args *ka)
{
//	int err;
	TOUCH(ka);

	dprintf("dev_init: entry\n");

//	bootstrap_devfs();

//	err = vfs_create(NULL, "/dev", "", STREAM_TYPE_DIR);
//	if(err < 0)
//		panic("dev_init: error making /dev!\n");
//	err = vfs_mount("/dev", "devfs");
//	if(err < 0)
//		panic("dev_init: error mounting devfs!\n");
	
	return 0;
}
