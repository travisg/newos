#include <kernel.h>
#include <devs.h>
#include <vfs.h>
#include <debug.h>

#include <string.h>

#include <con.h>

int devs_init(kernel_args *ka)
{
	TOUCH(ka);
/*
	console_dev_init(ka);

	{
		int fd;
		
		dprintf("devs_init: devfs test:\n");
		fd = vfs_opendir(NULL, "/dev");
		if(fd >= 0) {
			char buf[64];
			int buf_len;
			int err;
			struct dirent *de = (struct dirent *)buf;
	
			vfs_rewinddir(fd);
			for(;;) {
				buf_len = sizeof(buf);
				err = vfs_readdir(fd, buf, &buf_len);
				if(err < 0)
					dprintf(" readdir returned %d\n", err);
				if(buf_len > 0) 
					dprintf(" readdir returned name = '%s'\n", de->name);
				else
					break;
			}
			vfs_closedir(fd);
		}
		dprintf("devs_init: devfs test done\n");
	
		fd = vfs_open(NULL, "/dev/console");
		dprintf("/dev/console fd = %d\n", fd);
		if(fd >= 0) {
			size_t size = strlen("foo");
			
			vfs_write(fd, "foo", 0, &size);
		}
	}
*/	
	

	return 0;
}
