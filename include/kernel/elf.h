/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_ELF_H
#define _KERNEL_ELF_H

#include <kernel/thread.h>
#include <newos/elf32.h>

int elf_load_uspace(const char *path, struct proc *p, int flags, addr *entry);
image_id elf_load_kspace(const char *path, const char *sym_prepend);
int elf_unload_kspace( const char *path);
addr elf_lookup_symbol(image_id id, const char *symbol);
int elf_reverse_lookup_symbol(addr address, addr *base_address, char *text, int len);
int elf_init(kernel_args *ka);

/* internal stuff */
struct elf_region {
	region_id id;
	addr start;
	addr size;
	long delta;
};

struct elf_image_info {
	struct elf_image_info *next;
	image_id id;
	int ref_count;
	void *vnode;
	struct elf_region regions[2]; // describes the text and data regions
	addr dynamic_ptr; // pointer to the dynamic section
	struct elf_linked_image *linked_images;

	struct Elf32_Ehdr *eheader;

	// pointer to symbol participation data structures
	char *needed;
	unsigned int *symhash;
	struct Elf32_Sym *syms;
	char *strtab;
	struct Elf32_Rel *rel;
	int rel_len;
	struct Elf32_Rela *rela;
	int rela_len;
	struct Elf32_Rel *pltrel;
	int pltrel_len;
	int pltrel_type;
};

#define STRING(image, offset) ((char *)(&(image)->strtab[(offset)]))
#define SYMNAME(image, sym) STRING(image, (sym)->st_name)
#define SYMBOL(image, num) ((struct Elf32_Sym *)&(image)->syms[num])
#define HASHTABSIZE(image) ((image)->symhash[0])
#define HASHBUCKETS(image) ((unsigned int *)&(image)->symhash[2])
#define HASHCHAINS(image) ((unsigned int *)&(image)->symhash[2+HASHTABSIZE(image)])

int elf_resolve_symbol(struct elf_image_info *image, struct Elf32_Sym *sym, struct elf_image_info *shared_image, const char *sym_prepend,addr *sym_addr);

#endif

