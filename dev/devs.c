#include <kernel.h>
#include <devs.h>
#include <vfs.h>
#include <debug.h>

#include <string.h>

#include <con.h>
#include <null.h>
#include <zero.h>

int devs_init(kernel_args *ka)
{
	TOUCH(ka);

	console_dev_init(ka);
	
	null_dev_init(ka);
	zero_dev_init(ka);

#if 0
	{
		int fd;
		
		dprintf("devs_init: devfs test:\n");
		fd = vfs_open(NULL, "/dev", "", STREAM_TYPE_DIR);
		if(fd >= 0) {
			char buf[64];
			int buf_len;
			int err;
			struct dirent *de = (struct dirent *)buf;
	
			vfs_seek(fd, 0, SEEK_SET);
			for(;;) {
				buf_len = sizeof(buf);
				err = vfs_read(fd, buf, -1, &buf_len);
				if(err < 0)
					dprintf(" readdir returned %d\n", err);
				if(buf_len > 0) 
					dprintf(" readdir returned name = '%s'\n", de->name);
				else
					break;
			}
			vfs_close(fd);
		}
		dprintf("devs_init: devfs test done\n");
	
		fd = vfs_open(NULL, "/dev/console", "", STREAM_TYPE_DEVICE);
		dprintf("/dev/console fd = %d\n", fd);
		if(fd >= 0) {
			size_t size = strlen("foo");
			
			vfs_write(fd, "foo", 0, &size);
		}
	}
#endif	

	return 0;
}
