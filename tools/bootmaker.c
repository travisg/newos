/* $Id$
**
** Copyright 1998 Brian J. Swetland
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions, and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions, and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "../include/boot.h"

#include "bootblock.h"
#include "sparcbootblock.h"
#include "sh4bootblock.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int make_floppy = 0;
static int make_sparcboot = 0;
static int make_sh4boot = 0;

void die(char *s, char *a)
{
    fprintf(stderr,"error: ");
    fprintf(stderr,s,a);
    fprintf(stderr,"\n");
    exit(1);
}

void *loadfile(char *file, int *size)
{
    int fd;
    char *data;
    struct stat info;
    
    if((fd = open(file,O_RDONLY)) != -1){
        if(fstat(fd,&info)){
            close(fd);
            *size = 0;
            return NULL;
        }
        data = (char *) malloc(info.st_size);
        if(read(fd, data, info.st_size) != info.st_size) {
            close(fd);
            *size = 0;
            return NULL;
        }
        close(fd);
        *size = info.st_size;
        return data;
    }
    *size = 0;
    return NULL;
}

int writesh4bootblock(FILE *fp, unsigned int blocks)
{
	unsigned char bb[0x200];

	memset(bb, 0, sizeof(bb));
	memcpy(bb, sh4bootblock, sizeof(sh4bootblock));

	return fwrite(bb,sizeof(bb),1,fp);
}

// write a boot block to the head of the dir.
// note: the first 0x20 bytes are removed by the sparc prom
// which makes the whole file off by 0x20 bytes
int writesparcbootblock(FILE *fp, unsigned int blocks)
{
	unsigned char bb[0x200+0x20];

	memset(bb, 0, sizeof(bb));
	memcpy(bb, sparcbootblock, sizeof(sparcbootblock));

	return fwrite(bb,sizeof(bb),1,fp);
}

/* at location 2 is a uint16, set to blocks * 8 */
int writebootblock(FILE *fp, unsigned int blocks)
{
    unsigned char bb[512];
    
    blocks *= 8;
    
    memcpy(bb,bootblock,512);
    
    bb[2] = (blocks & 0x00FF);
    bb[3] = (blocks & 0xFF00) >> 8;
    
    fwrite(bb,512,1,fp);
}

typedef struct _nvpair 
{
    struct _nvpair *next;
    char *name;
    char *value;
} nvpair;


typedef struct _section
{
    struct _section *next;
    char *name;
    struct _nvpair *firstnv;
} section;

void print_sections(section *first)
{
    nvpair *p;
    
    while(first){
        printf("\n[%s]\n",first->name);
        for(p = first->firstnv; p; p = p->next){
            printf("%s=%s\n",p->name,p->value);
        }
        first = first->next;
    }
}

#ifdef xBIG_ENDIAN
unsigned int fix(unsigned int x)
{
    int r;
    unsigned char *a = (unsigned char *) &x;
    unsigned char b[4];

    b[0] = a[3];
    b[1] = a[2];
    b[2] = a[1];
    b[3] = a[0];

    r = *((unsigned int *)b);
    return r;
    
}
#else
#define fix(x) (x)
#endif

#define stNEWLINE 0
#define stSKIPLINE 1
#define stHEADER 2
#define stLHS 3
#define stRHS 4

section *first = NULL;
section *last = NULL;

section *load_ini(char *file)
{
    char *data,*end;
    int size;
    int state = stNEWLINE;
	section *cur;
	
    char *lhs,*rhs;
    
    if(!(data = loadfile(file,&size))){
        return NULL;
    }
    end = data+size;
    
    while(data < end){
        switch(state){
        case stSKIPLINE:
            if(*data == '\n'){
                state = stNEWLINE;
            }
            data++;
            break;
            
        case stNEWLINE:
            if(*data == '\n'){
                data++;
                break;
            }
            if(*data == '['){
                lhs = data+1;
                state = stHEADER;
                data++;
                break;
            }
            if(*data == '#' || *data <= ' '){
                state = stSKIPLINE;
                data++;
                break;
            }
            lhs = data;
            data++;
            state = stLHS;
            break;
        case stHEADER:        
            if(*data == ']'){                
                cur = (section *) malloc(sizeof(section));
                cur->name = lhs;
                cur->firstnv = NULL;
                cur->next = NULL;
                if(last){
                    last->next = cur;
                    last = cur;
                } else {
                    last = first = cur;
                }
                *data = 0;
                state = stSKIPLINE;
            }
            data++;
            break;
        case stLHS:
            if(*data == '\n'){
                state = stNEWLINE;
            }
            if(*data == '='){
                *data = 0;
                rhs = data+1;
                state = stRHS;
            }
            data++;
            continue;
        case stRHS:
            if(*data == '\n'){
                nvpair *p = (nvpair *) malloc(sizeof(nvpair));
                p->name = lhs;
                p->value = rhs;
                *data = 0;
                p->next = cur->firstnv;
                cur->firstnv = p;
                state = stNEWLINE;
            }
            data++;
            break;
        }
    }
    return first;
    
}


char *getval(section *s, char *name)
{
    nvpair *p;
    for(p = s->firstnv; p; p = p->next){
        if(!strcmp(p->name,name)) return p->value;
    }
    return NULL;
}

char *getvaldef(section *s, char *name, char *def)
{
    nvpair *p;
    for(p = s->firstnv; p; p = p->next){
        if(!strcmp(p->name,name)) return p->value;
    }
    return def;
}

#define centry bdir.bd_entry[c]
void makeboot(section *s, char *outfile)
{
    FILE *fp;
    void *rawdata[64];
    int rawsize[64];
    char fill[4096];
    boot_dir bdir;
    int i,c;
    int nextpage = 1; /* page rel offset of next loaded object */

    memset(fill,0,4096);
    
    memset(&bdir, 0, 4096);
    for(i=0;i<64;i++){
        rawdata[i] = NULL;
        rawsize[i] = 0;
    }

    c = 1;

    bdir.bd_entry[0].be_type = fix(BE_TYPE_DIRECTORY);
    bdir.bd_entry[0].be_size = fix(1);
    bdir.bd_entry[0].be_vsize = fix(1);
    rawdata[0] = (void *) &bdir;
    rawsize[0] = 4096;
    
    strcpy(bdir.bd_entry[0].be_name,"SBBB/Directory");

    while(s){
        char *type = getvaldef(s,"type","NONE");
        char *file = getval(s,"file");
        int vsize;
        int size;
        struct stat statbuf;
        
        if(!type) die("section %s has no type",s->name);

        strncpy(centry.be_name,s->name,32);
        centry.be_name[31] = 0;

        if(!file) die("section %s has no file",s->name);
        if(!(rawdata[c] = loadfile(file,&rawsize[c])))
           die("cannot load \"%s\"",file);

        if(stat(file,&statbuf))
            die("cannot stat \"%s\"",file);
        vsize = statbuf.st_size;
        
        centry.be_size = rawsize[c] / 4096 + (rawsize[c] % 4096 ? 1 : 0);
        centry.be_vsize = 
            (vsize < centry.be_size) ? centry.be_size : vsize;

        centry.be_offset = nextpage;
        nextpage += centry.be_size;

        centry.be_size = fix(centry.be_size);
        centry.be_vsize = fix(centry.be_vsize);
        centry.be_offset = fix(centry.be_offset);
        
        if(!strcmp(type,"boot")){
            centry.be_type = fix(BE_TYPE_BOOTSTRAP);
            centry.be_code_vaddr = fix(atoi(getvaldef(s,"vaddr","0")));
            centry.be_code_ventr = fix(atoi(getvaldef(s,"ventry","0")));            
        }
        if(!strcmp(type,"code")){
            centry.be_type = fix(BE_TYPE_CODE);
            centry.be_code_vaddr = fix(atoi(getvaldef(s,"vaddr","0")));
            centry.be_code_ventr = fix(atoi(getvaldef(s,"ventry","0"))); 
        }
        if(!strcmp(type,"data")){
            centry.be_type = fix(BE_TYPE_DATA);
        }
        if(!strcmp(type,"elf32")){
            centry.be_type = fix(BE_TYPE_ELF32);
        }

        if(centry.be_type == BE_TYPE_NONE){
            die("unrecognized section type \"%s\"",type);
        }

        c++;
        s = s->next;
        
        if(c==64) die("too many sections (>63)",NULL);
    }

    if(!(fp = fopen(outfile,"w"))){
        die("cannot write to \"%s\"",outfile);
    }

    if(make_sh4boot) {
        writesh4bootblock(fp, nextpage+1);
    }
    
    if(make_sparcboot) {
        writesparcbootblock(fp, nextpage+1);
    }
    
    if(make_floppy) {
        writebootblock(fp, nextpage+1);
    }
    
    for(i=0;i<c;i++){
        fwrite(rawdata[i],rawsize[i],1,fp);
        if(rawsize[i]%4096) fwrite(fill,4096 - (rawsize[i]%4096),1,fp);
    }
    fclose(fp);
    
    
}

int main(int argc, char **argv)
{
	char *file = NULL;
    section *s;
    
    if(argc < 2){
usage:
        fprintf(stderr,"usage: %s [ --floppy | -f ] [ --sparc | -s ] [ --sh4 | -4 ] [ <inifile> ... ] -o <bootfile>\n",argv[0]);
        return 1;
    }

	argc--;
	argv++;
	
	while(argc){
		if(!strcmp(*argv,"--floppy")){
			make_floppy = 1;
		} if(!strcmp(*argv,"--sparc")) {
			make_sparcboot = 1;
		} if(!strcmp(*argv,"--sh4")) {
			make_sh4boot = 1;
		} else if(!strcmp(*argv,"-o")){
			argc--;
			argv++;
			if(argc){
				file = *argv;
			} else {
				goto usage;
			}
		} else {
			if(load_ini(*argv) == NULL){
				fprintf(stderr,"warning: cannot load '%s'\n",*argv);
			}
		}
		argc--;
		argv++;
	}
	
	
    if((argc > 3) && !strcmp(argv[3],"-floppy")){
        make_floppy = 1;
    }
    if((argc > 3) && !strcmp(argv[3],"-sparc")){
        make_sparcboot = 1;
    }
    if((argc > 3) && !strcmp(argv[3],"-sh4")){
        make_sh4boot = 1;
    }

	if(!file){
		fprintf(stderr,"error: no output specified\n");
		goto usage;
	}
	
	if(!first){
		fprintf(stderr,"error: no data to write?!\n");
		goto usage;
	}
	
	makeboot(first,file);
    
    return 0;    
}

