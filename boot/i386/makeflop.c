#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "bootblock.h"

int main(int argc, char *argv[])
{
	struct stat st;
	int err;
	unsigned int blocks;
	char buf[512];
	size_t read_size;
	int filefd;
	
	if(argc < 2)
		return -1;

	err = stat(argv[1], &st);
	if(err < 0)
		return -1;

	filefd = open(argv[1], O_RDONLY);
	if(filefd < 0)
		return -1;

	// patch the size of the output into bytes 3 & 4 of the bootblock
	blocks = st.st_size / 512;
	blocks++;
	bootblock[2] = (blocks & 0x00ff);
	bootblock[3] = (blocks & 0xff00) >> 8;
	
	write(1, bootblock, sizeof(bootblock));
	
	while((read_size = read(filefd, buf, sizeof(buf))) > 0) {
		write(1, buf, read_size);
	}

	close(filefd);

	return 0;
}

