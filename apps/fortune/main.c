#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libsys/stdio.h>
#include <sys/syscalls.h>

#define FORTUNES "/boot/etc/fortunes"

int
main(void)
{
	int fd;
	int rc;
	char *buf;
	unsigned i;
	unsigned found;
	struct file_stat stat;

	rc = sys_rstat(FORTUNES, &stat);
	if(rc< 0) {
		printf("Cookie monster was here!!!\n");
		sys_exit(1);
	}

	buf= malloc(stat.size+1);

	fd= open(FORTUNES, 0);
	read(fd, buf, stat.size);
	buf[stat.size]= 0;
	close(fd);

	found= 0;
	for(i= 0; i< stat.size; i++) {
		if(strncmp(buf+i, "#@#", 3)== 0) {
			found+= 1;
		}
	}

	found= 1+(sys_system_time()%found);

	for(i= 0; i< stat.size; i++) {
		if(strncmp(buf+i, "#@#", 3)== 0) {
			found-= 1;
		}
		if(found== 0) {
			unsigned j;

			for(j= i+1; j< stat.size; j++) {
				if(strncmp(buf+j, "#@#", 3)== 0) {
					buf[j]= 0;
				}
			}

			printf("%s\n", buf+i+3);
			break;
		}
	}

	return 0;
}
