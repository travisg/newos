/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <sys/errors.h>
#include <kernel/elf.h>
#include <kernel/vfs.h>
#include <kernel/vm.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/arch/cpu.h>

#include <sys/elf32.h>

#include <libc/string.h>
#include <libc/printf.h>

struct elf_region {
	region_id id;
	addr start;
	addr size;
};

struct elf_image_info {
	struct elf_image_info *next;
	struct elf_region regions[2]; // describes the text and data regions
	addr dynamic_ptr; // pointer to the dynamic section

	// pointer to symbol participation data structures
	unsigned int *symhash;
	struct Elf32_Sym *syms;
	char *strtab;
};

static struct elf_image_info *kernel_images = NULL;
static struct elf_image_info *kernel_image = NULL;

#define SYMNAME(image, sym) ((char *)(&(image)->strtab[(sym)->st_name]))
#define HASHTABSIZE(image) ((image)->symhash[0])
#define HASHBUCKETS(image) ((unsigned int *)&(image)->symhash[2])
#define HASHCHAINS(image) ((unsigned int *)&(image)->symhash[2+HASHTABSIZE(image)])

static unsigned long elf_hash(const unsigned char *name)
{
	unsigned long hash = 0;
	unsigned long temp;

	while(*name) {
		hash = (hash << 4) + *name++;
		if((temp = hash & 0xf0000000))
			hash ^= temp >> 24;
		hash &= ~temp;
	}
	return hash;
}

static int verify_eheader(struct Elf32_Ehdr *eheader)
{
	if(memcmp(eheader->e_ident, ELF_MAGIC, 4) != 0)
		return ERR_INVALID_BINARY;

	if(eheader->e_ident[4] != ELFCLASS32)
		return ERR_INVALID_BINARY;

	if(eheader->e_phoff == 0)
		return ERR_INVALID_BINARY;

	if(eheader->e_phentsize < sizeof(struct Elf32_Phdr))
		return ERR_INVALID_BINARY;

	return 0;
}

int elf_load_uspace(const char *path, struct proc *p, int flags, addr *entry)
{
	struct Elf32_Ehdr eheader;
	struct Elf32_Phdr *pheaders;
	int fd;
	int err;
	int i;
	ssize_t len;

	dprintf("elf_load: entry path '%s', proc 0x%x\n", path, p);

	fd = sys_open(path, STREAM_TYPE_FILE, 0);
	if(fd < 0)
		return fd;

	len = sys_read(fd, &eheader, 0, sizeof(eheader));
	if(len < 0) {
		err = len;
		goto error;
	}
	if(len != sizeof(eheader)) {
		// short read
		err = ERR_INVALID_BINARY;
		goto error;
	}
	err = verify_eheader(&eheader);
	if(err < 0)
		goto error;

	pheaders = kmalloc(eheader.e_phnum * eheader.e_phentsize);
	if(pheaders == NULL) {
		dprintf("error allocating space for program headers\n");
		err = ERR_NO_MEMORY;
		goto error;
	}

	dprintf("reading in program headers at 0x%x, len 0x%x\n", eheader.e_phoff, eheader.e_phnum * eheader.e_phentsize);
	len = sys_read(fd, pheaders, eheader.e_phoff, eheader.e_phnum * eheader.e_phentsize);
	if(len < 0) {
		err = len;
		dprintf("error reading in program headers\n");
		goto error;
	}
	if(len != eheader.e_phnum * eheader.e_phentsize) {
		dprintf("short read while reading in program headers\n");
		err = -1;
		goto error;
	}

	for(i=0; i < eheader.e_phnum; i++) {
		char region_name[64];
		region_id id;
		char *region_addr;

		sprintf(region_name, "%s_seg%d", path, i);

		region_addr = (char *)ROUNDOWN(pheaders[i].p_vaddr, PAGE_SIZE);
		id = vm_create_anonymous_region(p->aspace_id, region_name, (void **)&region_addr, REGION_ADDR_EXACT_ADDRESS,
			ROUNDUP(pheaders[i].p_memsz + (pheaders[i].p_vaddr % PAGE_SIZE), PAGE_SIZE), REGION_WIRING_LAZY, LOCK_RW);
		if(id < 0) {
			dprintf("error allocating region!\n");
			err = ERR_INVALID_BINARY;
			goto error;
		}

		len = sys_read(fd, region_addr + (pheaders[i].p_vaddr % PAGE_SIZE), pheaders[i].p_offset, pheaders[i].p_filesz);
		if(len < 0) {
			err = len;
			dprintf("error reading in seg %d\n", i);
			goto error;
		}

		if(i == 0)
			*entry = pheaders[i].p_vaddr;
	}

	dprintf("elf_load: done!\n");

	err = 0;

error:
	sys_close(fd);

	return err;
}

static int elf_parse_dynamic_section(struct elf_image_info *image)
{
	struct Elf32_Dyn *d;
	int i;

	image->symhash = 0;
	image->syms = 0;
	image->strtab = 0;

	d = (struct Elf32_Dyn *)image->dynamic_ptr;
	if(!d)
		return ERR_GENERAL;

	for(i=0; d[i].d_tag != DT_NULL; i++) {
		switch(d[i].d_tag) {
			case DT_HASH:
				image->symhash = (unsigned int *)d[i].d_un.d_ptr;
				break;
			case DT_STRTAB:
				image->strtab = (char *)d[i].d_un.d_ptr;
				break;
			case DT_SYMTAB:
				image->syms = (struct Elf32_Sym *)d[i].d_un.d_ptr;
				break;
			default:
				continue;
		}
	}

	// lets make sure we found all the required sections
	if(!image->symhash || !image->syms || !image->strtab)
		return ERR_GENERAL;

	return NO_ERROR;
}

static struct Elf32_Sym *elf_find_symbol(struct elf_image_info *image, const char *name)
{
	unsigned int hash;
	unsigned int i;

	hash = elf_hash(name) % HASHTABSIZE(image);
	for(i = HASHBUCKETS(image)[hash]; i != STN_UNDEF; i = HASHCHAINS(image)[i]) {
		if(!strcmp(SYMNAME(image, &image->syms[i]), name)) {
			return &image->syms[i];
		}
	}

	return NULL;
}

int elf_init(kernel_args *ka)
{
	vm_region_info rinfo;
	int err;

	// build a image structure for the kernel, which has already been loaded
	kernel_image = kmalloc(sizeof(struct elf_image_info));
	memset(kernel_image, 0, sizeof(struct elf_image_info));

	// text segment
	kernel_image->regions[0].id = vm_find_region_by_name(vm_get_kernel_aspace_id(), "kernel_ro");
	if(kernel_image->regions[0].id < 0)
		panic("elf_init: could not look up kernel text segment region\n");
	vm_get_region_info(kernel_image->regions[0].id, &rinfo);
	kernel_image->regions[0].start = rinfo.base;
	kernel_image->regions[0].size = rinfo.size;

	// data segment
	kernel_image->regions[1].id = vm_find_region_by_name(vm_get_kernel_aspace_id(), "kernel_rw");
	if(kernel_image->regions[1].id < 0)
		panic("elf_init: could not look up kernel data segment region\n");
	vm_get_region_info(kernel_image->regions[1].id, &rinfo);
	kernel_image->regions[1].start = rinfo.base;
	kernel_image->regions[1].size = rinfo.size;

	// we know where the dynamic section is
	kernel_image->dynamic_ptr = (addr)ka->kernel_dynamic_section_addr.start;

	// parse the dynamic section
	if(elf_parse_dynamic_section(kernel_image) < 0)
		panic("elf_init: elf_parse_dynamic_section doesn't like the kernel image\n");

	// insert it first in the list of kernel images loaded
	kernel_image->next = NULL;
	kernel_images = kernel_image;

	return 0;
}

