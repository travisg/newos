/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static unsigned char bootblock[] = {
#include "bootblock.h"
};

#ifndef O_BINARY
#define O_BINARY 0
#endif

int main(int argc, char *argv[])
{
	struct stat st;
	int err;
	unsigned int blocks;
	char buf[512];
	size_t read_size;
	int filefd;
	int outfd;
	
	if(argc < 3)
		return -1;

	err = stat(argv[1], &st);
	if(err < 0)
		return -1;

	filefd = open(argv[1], O_BINARY|O_RDONLY);
	if(filefd < 0) {
		printf("error: cannot open input file '%s'\n", argv[1]);
		return -1;
	}

	outfd = open(argv[2], O_BINARY|O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if(outfd < 0) {
		printf("error: cannot open output file '%s'\n", argv[2]);
		return -1;
	}

	// patch the size of the output into bytes 3 & 4 of the bootblock
	blocks = st.st_size / 512;
	blocks++;
	bootblock[2] = (blocks & 0x00ff);
	bootblock[3] = (blocks & 0xff00) >> 8;
	
	write(outfd, bootblock, sizeof(bootblock));
	
	while((read_size = read(filefd, buf, sizeof(buf))) > 0) {
		write(outfd, buf, read_size);
	}

	close(outfd);
	close(filefd);

	return 0;
}

