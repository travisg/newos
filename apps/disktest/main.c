/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* default mask */
uint32 xor_mask = 0x23fe9a02;

static short htons(short value)
{
	return ((value>>8)&0xff) | ((value&0xff)<<8);
}

static int htonl(int value)
{
	return htons((value>>16)&0xffff) | (htons(value&0xffff)<<16);
}

/* bit reordering map for encoding. decoding is the opposite of this */
int map[32] = {
	31, 23, 15,  7, 24, 16,  8,  0,
	30, 22, 14,  6, 25, 17,  9,  1,
	26, 18, 10,  2, 29, 28, 13,  5,
	27, 19, 11,  3, 21, 20, 12,  4
};

struct
{
	uint32 mask;   /* mask off the bit to shift */
	uint32 dest;   /* bit to turn on */
} encode_map[32], decode_map[32];

static void makemaps(void)
{
	int i;
	uint32 src,dst;

	for(i=0;i<32;i++) {
		src = i;
		dst = map[i];

		encode_map[src].mask = 1 << src;
		encode_map[src].dest = 1 << dst;
		decode_map[dst].mask = 1 << dst;
		decode_map[dst].dest = 1 << src;
	}
}

static uint32 encode(uint32 n)
{
	int i;
	uint32 r = 0;

	n = n ^ xor_mask;

	for(i=0;i<32;i++){
		if(n & encode_map[i].mask) r |= encode_map[i].dest;
	}
	return r;
}

static uint32 decode(uint32 n)
{
	int i;
	uint32 r = 0;

	for(i=0;i<32;i++){
		if(n & decode_map[i].mask) r |= decode_map[i].dest;
	}
	return r ^ xor_mask;
}

/*
** Fill a buffer 'length' bytes long that will be written at start
** offset 'startbyte' with the pattern.
*/
static void create_buffer(uint32 startbyte, uint32 length, uchar *buffer)
{
	uint32 data;

	if(startbyte % 4){
		int l = 4 - (startbyte % 4);
		if(l > length) l = length;
			data = htonl(encode(startbyte/4));
		memcpy(buffer, ((unsigned char *)&data) + startbyte%4, l);
		length -= l;
		buffer += l;
		startbyte += l;
	}
	while(length > 4){
		data = htonl(encode(startbyte/4));
		memcpy(buffer, &data, 4);
		buffer += 4;
		startbyte += 4;
		length -= 4;
	}
	if(length){
		data = htonl(encode(startbyte/4));
		memcpy(buffer, &data, length);
	}
}

/*
** Check a buffer of data that originates at 'startbyte' in the file
** to make sure it matches the data that create_buffer would have
** generated.
*/
static int check_buffer(uint32 startbyte, uint32 length, uchar *buffer, uint32 *err)
{
	uint32 data;

	if(startbyte % 4){
		int l = 4 - (startbyte % 4);
		if(l > length) l = length;
		data = htonl(encode(startbyte/4));
		if(memcmp(buffer, ((unsigned char *)&data) + startbyte%4, l)) {
			*err = startbyte;
			return 1;
		}
		length -= l;
		buffer += l;
		startbyte += l;
	}
	while(length > 4){
		data = htonl(encode(startbyte/4));
		if(memcmp(buffer, &data, 4)) {
			*err = startbyte;
			return 1;
		}
		buffer += 4;
		startbyte += 4;
		length -= 4;
	}
	if(length){
		data = htonl(encode(startbyte/4));
		if(memcmp(buffer, &data, length)){
			*err = startbyte;
			return 1;
		}
	}
	return 0;
}

off_t number(char *s)
{
	if(!s || !*s) {
		return 0;
	} else {
		int n = atoul(s);
		switch(s[strlen(s)-1]){
		case 'G':
		case 'g':
			n *= 1024;
		case 'M':
		case 'm':
			n *= 1024;
		case 'k':
		case 'K':
			n *= 1024;
			break;
		}
		return n;
	}
}

int maxblock = 0;
int minblock = 0;
off_t maxfilesize = 0;
int nogrow = 0;
int prefill = 0;
int preverify = 0;
int readpercent = 0;
int align = 1;
int haltonerror = 0;
char *filename = NULL;
int verbose = 0;

int setalign(char *val)
{
	align = number(val);
	fprintf(stderr,"foo = %d\n",align);
	return 0;
}

int setxor(char *val)
{
	xor_mask = number(val);
	fprintf(stderr,"xor mask = %d\n",xor_mask);
	return 0;
}

int setreadpercent(char *val)
{
	readpercent = atoi(val);
	if(readpercent < 0 || readpercent > 100) {
		fprintf(stderr,"error: invalid readpercent of %d\n",readpercent);
		return 1;
	}
	return 0;
}

int sethaltonerror(char *val)
{
	haltonerror = 1;
	return 0;
}

int setverbose(char *val)
{
	verbose = 1;
	return 0;
}

int setnogrow(char *val)
{
	nogrow = 1;
	return 0;
}

int setprefill(char *val)
{
	prefill = 1;
	return 0;
}

int setpreverify(char *val)
{
	preverify = 1;
	return 0;
}

int setfile(char *val)
{
	if(!val || !*val){
		fprintf(stderr,"error: must specify a data file\n");
		return 1;
	}
	filename = val;
	return 0;
}

int setmaxfilesize(char *val)
{
	maxfilesize = number(val);
	if(maxfilesize < 1) {
		fprintf(stderr,"error: maxfilesize must be >= 1\n");
		return 1;
	}
	return 0;
}

int setmaxblock(char *val)
{
	maxblock = number(val);
	if(maxblock < 1) {
		fprintf(stderr,"error: maxblock must be >= 1\n");
		return 1;
	}
	return 0;
}

int setminblock(char *val)
{
	minblock = number(val);
	if(minblock < 1) {
		fprintf(stderr,"error: minblock must be >= 1\n");
		return 1;
	}
	return 0;
}


struct
{
	char *name;
	char *defaultvalue;
	int set;
	int (*handler)(char *val);
	char *info;
} argmap[] = {
	{ "-file=", NULL, 0, setfile,
	  "file to operate on" },
	{ "-maxfilesize=", "524288", 0, setmaxfilesize,
	  "do not grow beyond this size" },
	{ "-maxblock=", "65536", 0, setmaxblock,
	  "largest block of data to write in one go" },
	{ "-minblock=", "1", 0, setminblock,
	  "smallest block of data to write in one go" },
	{ "-nogrow", NULL, 1, setnogrow,
	  "don't extend the filesize ever" },
	{ "-prefill", NULL, 1, setprefill,
	  "fill the file out to the maxsize on startup" },
	{ "-readpercent=", "50", 0, setreadpercent,
	  "percentage of reads vs writes to do" },
	{ "-haltonerror", NULL, 1, sethaltonerror,
	  "stop if an error occurs" },
	{ "-verbose", NULL, 1, setverbose,
	  "extreme verbosity mode" },
	{ "-align=", NULL, 1, setalign,
	  "alignment of blocks and blocksizes" },
	{ "-xor=", NULL, 1, setxor,
	  "set the xor mask" },
	{ "-preverify", NULL, 1, setpreverify,
	  "pre-verify the contents of the file before proceeding" },
	{ NULL, NULL, 0 }
};


void usage(void)
{
	int i;
	fprintf(stderr,"usage:\n");
	for(i=0;argmap[i].name;i++){
		fprintf(stderr,"\t%s",argmap[i].name);
		if(argmap[i].name[strlen(argmap[i].name)-1]=='=') {
			fprintf(stderr,"<value>");
			if(argmap[i].defaultvalue){
				fprintf(stderr,"  (default = %s)",argmap[i].defaultvalue);
			}
		}
		fprintf(stderr,"\n\t\t\"%s\"\n",argmap[i].info);
	}
}


uint32 R(int _min, int _max)
{
	int n = _max - _min;
#ifdef WIN32
	return (rand() + (rand() << 16)) % (n+1) + _min;
#else
	return _min + (((double) n)*rand()/(RAND_MAX+1.0));
#endif
}

void bye(void)
{

//	fprintf(stderr,"*** error exit after %ds ***\n",time(NULL)-starttime);
	fprintf(stderr,"*** error exit ***\n");
	exit(1);
}

void disktest()
{
	uchar *data;
	uint32 err;
	int fd;
	int start;
	int len;
	int dir;


	fprintf(stderr,"maxblock = %d, minblock = %d, align = %d\n",maxblock,minblock,align);
	if(!(data = malloc(maxblock))){
		fprintf(stderr,"error: could allocate %d byte buffer\n",maxblock);
		exit(1);
	}

    if((fd = open(filename, O_RDWR|O_CREAT)) < 0){
		fprintf(stderr,"error: cannot open \"%s\" for readwrite\n",filename);
		exit(1);
    }

    if(prefill){
		off_t percent = 0;
		int w;
		off_t len = maxfilesize;
		off_t pos = 0;
		fprintf(stderr,"status: filling file to %Ld bytes\n",maxfilesize);
		fprintf(stderr,"percent done:   0%%");
		while(len){
			create_buffer(pos, maxblock, data);
			w = len < maxblock ? len : maxblock;
			if(write(fd, data, w) != w){
				fprintf(stderr,"error: write @ %Ld, l=%d failed\n",pos,w);
				exit(1);
			}

			pos += w;
			len -= w;

			if(((pos*100)/maxfilesize) > percent) {
				percent = (pos * 100) / maxfilesize;
				fprintf(stderr,"\rpercent done: %3Ld", percent);
				fflush(stderr);
			}
		}
		fprintf(stderr,"\n");
	}

	if(preverify){
		off_t percent = 0;
        off_t len = maxfilesize;
        off_t pos = 0;
		int r, result;
		off_t fpos;

        fprintf(stderr,"\nstatus: pre-verifying file of %Ld bytes\n",maxfilesize);
		fflush(stderr);
        fprintf(stderr,"percent done:   0%%");
		fpos = lseek(fd, 0, SEEK_SET);
        while(len){
			r = len < maxblock ? len : maxblock;
            if((result = read(fd, data, r)) != r){
                fprintf(stderr," READ ERROR @ pos %Ld (sz %d, result)\n", pos, r, result);
                if(haltonerror) bye();
                continue;
            }
            if(check_buffer(pos,r,data,&err)){
                fprintf(stderr, "PRE-VERIFY ERR pos 0x%Lx size %d  ", pos, r);
                fprintf(stderr," VERIFY ERROR @ %08x\n",err);
                if(haltonerror) bye();
                continue;
            }
            pos += r;
            len -= r;


            if(((pos*100)/maxfilesize) > percent) {
                percent = (pos * 100) / maxfilesize;
                fprintf(stderr,"\rpercent done: %3Ld", percent);
                fflush(stderr);
            }
        }
        fprintf(stderr,"\n");

	}

	for(;;){
		start = R(0,maxfilesize);
		if(align > 1) start = (start/align)*align;

		len = R(minblock,maxblock > maxfilesize-start ? maxfilesize-start : maxblock);
		if(align > 1) len = (len/align)*align;

		if(len < minblock) continue;
		dir = R(0,100) < readpercent;

#define LOCATION() fprintf(stderr,"%s %08X @%08X - ",dir?"RD":"WR",len,start)
		if(verbose) LOCATION();
		else fprintf(stderr,dir ? "r" : "w");

		if(lseek(fd, start, SEEK_SET) < 0) {
			if(!verbose) LOCATION();
			fprintf(stderr," SEEK ERROR\n");
			if(haltonerror) bye();
			continue;
		}
		if(dir){
			if(read(fd, data, len) != len){
				if(!verbose) LOCATION();
				fprintf(stderr," READ ERROR\n");
				if(haltonerror) bye();
				continue;
			}
			if(check_buffer(start,len,data,&err)){
				if(!verbose) LOCATION();
				fprintf(stderr," VERIFY ERROR @ %08x\n",err);
				if(haltonerror) bye();
				continue;
			}
		} else {
			create_buffer(start,len,data);
			if(write(fd, data, len) != len){
				if(!verbose) LOCATION();
				fprintf(stderr," WRITE ERROR\n");
				if(haltonerror) bye();
				continue;
			}
		}

		if(verbose) fprintf(stderr," OKAY\n");

	}
    close(fd);
}



int main(int argc, char *argv[])
{
	int i,j;
	char *err;

//	starttime = time(NULL);

	makemaps();

	for(i=1;i<argc;i++){
		for(j=0;argmap[j].name;j++){
			if(!strncmp(argmap[j].name,argv[i],strlen(argmap[j].name))){
				if(argmap[j].handler(argv[i] + strlen(argmap[j].name))){
					usage();
					exit(1);
				}
				argmap[j].set = 1;
				break;
			}
		}
		if(!argmap[j].name){
			fprintf(stderr,"error: argument \"%s\" not understood\n",argv[i]);
			usage();
			exit(1);
		}
	}
	for(i=0;argmap[i].name;i++){
		if(!argmap[i].set) {
			if(argmap[i].handler(argmap[i].defaultvalue)){
				usage();
				exit(1);
			}
		}
	}

	disktest();

	return 0;
}



#if 0
static int initialize_device(int fd)
{
	char buf[18*1024];
	int i;
	ssize_t err;

	for(i=0; i < 1440*1024; i += sizeof(buf)) {
		create_buffer(i, sizeof(buf), buf);
		printf("writing at pos %d...", i);
		err = write(fd, buf, sizeof(buf));
		if(err < 0) {
			printf("error writing to disk\n");
			return -1;
		}
		printf("done\n");
	}

	return 0;
}

int main(int argc,char *argv[])
{
	int fd;
	const char *devname;

	if(argc < 2)
		devname = "/dev/disk/floppy/0";
	else
		devname = argv[1];

	// open the floppy device first
	fd = open(devname, O_RDWR);
	if(fd < 0) {
		printf("error opening device '%s', aborting...\n", devname);
		return -1;
	}

	makemaps();

	// initialize the device
	initialize_device(fd);

	// verify
	fprintf(stderr,

	close(fd);

	return 0;
}
#endif
