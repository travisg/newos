/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <sys/errors.h>
#include <kernel/elf.h>
#include <kernel/vfs.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/arch/cpu.h>

#include <sys/elf32.h>

#include <libc/string.h>
#include <libc/printf.h>

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

int elf_load(const char *path, struct proc *p, int flags, addr *entry)
{
	struct Elf32_Ehdr eheader;
	struct Elf32_Phdr *pheaders;
	int fd;
	int err;
	int i;
	ssize_t len;
	
	dprintf("elf_load: entry path '%s', proc 0x%x\n", path, p);
	
	fd = sys_open(path, "", STREAM_TYPE_FILE);
	if(fd < 0)
		return fd;

	len = sizeof(eheader);
	err = sys_read(fd, &eheader, 0, &len);
	if(err < 0)
		goto error;
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
	
	len = eheader.e_phnum * eheader.e_phentsize;
	dprintf("reading in program headers at 0x%x, len 0x%x\n", eheader.e_phoff, len);
	err = sys_read(fd, pheaders, eheader.e_phoff, &len);
	if(err < 0) {
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
		
		len = pheaders[i].p_filesz;
		err = sys_read(fd, region_addr + (pheaders[i].p_vaddr % PAGE_SIZE), pheaders[i].p_offset, &len);
		if(err < 0) {
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
