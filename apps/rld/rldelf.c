/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <string.h>
#include <sys/errors.h>
#include <sys/elf32.h>
#include <sys/syscalls.h>
#include <libsys/stdio.h>
#include <libc/printf.h>

#include "rld_priv.h"


#define	PAGE_SIZE 4096
#define	PAGE_MASK ((PAGE_SIZE)-1)
#define	PAGE_OFFS(y) ((y)&(PAGE_MASK))
#define	PAGE_BASE(y) ((y)&~(PAGE_MASK))

#define	ROUNDOWN(x,y) ((x)&~((y)-1))
#define	ROUNDUP(x,y) ROUNDOWN(x+y-1,y)


enum {
	RFLAG_RW  = 0x0001,
	RFLAG_ANON= 0x0002,
	RFLAG_SYMBOLIC= 0x1000,
	RFLAG_PROTECTED= 0x2000,
	RFLAG_INITIALIZED= 0x4000,
	RFLAG_NEEDAGIRLFRIEND= 0x8000
};


typedef
struct elf_region_t {
	region_id id;
	addr start;
	addr size;
	addr vmstart;
	addr vmsize;
	addr fdstart;
	addr fdsize;
	long delta;
	unsigned flags;
} elf_region_t;


typedef
struct image_t {
	struct   image_t *next;
	struct   image_t **prev;
	int      ref_count;

	addr entry_point;
	addr dynamic_ptr; // pointer to the dynamic section


	// pointer to symbol participation data structures
	unsigned int      *symhash;
	struct Elf32_Sym  *syms;
	char              *strtab;
	struct Elf32_Rel  *rel;
	int                rel_len;
	struct Elf32_Rela *rela;
	int                rela_len;
	struct Elf32_Rel  *pltrel;
	int                pltrel_len;

	unsigned           num_needed;
	struct image_t   **needed;

	// describes the text and data regions
	unsigned     num_regions;
	elf_region_t regions[1];
} image_t;


typedef
struct image_queue_t {
	image_t *head;
	image_t *tail;
} image_queue_t;


static image_queue_t loaded_images= { 0, 0 };



#define STRING(image, offset) ((char *)(&(image)->strtab[(offset)]))
#define SYMNAME(image, sym) STRING(image, (sym)->st_name)
#define SYMBOL(image, num) ((struct Elf32_Sym *)&(image)->syms[num])
#define HASHTABSIZE(image) ((image)->symhash[0])
#define HASHBUCKETS(image) ((unsigned int *)&(image)->symhash[2])
#define HASHCHAINS(image) ((unsigned int *)&(image)->symhash[2+HASHTABSIZE(image)])


/*
 * This macro is non ISO compliant, but a gcc extension
 */
#define	FATAL(x,y...) \
	if(x) { \
		printf("rld.so: " y); \
		sys_exit(0); \
	}



static
void
enqueue(image_t *img)
{
	img->next= 0;

	if(loaded_images.tail) {
		loaded_images.tail->next= img;
	}
	loaded_images.tail= img;
	if(!loaded_images.head) {
		loaded_images.head= img;
	}
}

static
unsigned long
elf_hash(const unsigned char *name)
{
	unsigned long hash = 0;
	unsigned long temp;

	while(*name) {
		hash = (hash << 4) + *name++;
		if((temp = hash & 0xf0000000)) {
			hash ^= temp >> 24;
		}
		hash &= ~temp;
	}
	return hash;
}

static
int
parse_eheader(struct Elf32_Ehdr *eheader)
{
	if(memcmp(eheader->e_ident, ELF_MAGIC, 4) != 0)
		return ERR_INVALID_BINARY;

	if(eheader->e_ident[4] != ELFCLASS32)
		return ERR_INVALID_BINARY;

	if(eheader->e_phoff == 0)
		return ERR_INVALID_BINARY;

	if(eheader->e_phentsize < sizeof(struct Elf32_Phdr))
		return ERR_INVALID_BINARY;

	return eheader->e_phentsize*eheader->e_phnum;
}

static
int
count_regions(char const *buff, int phnum, int phentsize)
{
	int i;
	int retval;
	struct Elf32_Phdr *pheaders;

	retval= 0;
	for(i= 0; i< phnum; i++) {
		pheaders= (struct Elf32_Phdr *)(buff+i*phentsize);

		switch(pheaders->p_type) {
			case PT_NULL:
				/* NOP header */
				break;
			case PT_LOAD:
				retval+= 1;
				if(pheaders->p_memsz!= pheaders->p_filesz) {
					unsigned A= pheaders->p_vaddr+pheaders->p_memsz;
					unsigned B= pheaders->p_vaddr+pheaders->p_filesz;

					A= PAGE_BASE(A);
					B= PAGE_BASE(B);

					if(A!= B) {
						retval+= 1;
					}
				}
				break;
			case PT_DYNAMIC:
				/* will be handled at some other place */
				break;
			case PT_INTERP:
				/* should check here for appropiate interpreter */
				break;
			case PT_NOTE:
				/* unsupported */
				break;
			case PT_SHLIB:
				/* undefined semantics */
				break;
			case PT_PHDR:
				/* we don't use it */
				break;
			default:
				FATAL(true, "unhandled pheader type 0x%x\n", pheaders[i].p_type);
				break;
		}
	}

	return retval;
}

static
image_t *
create_image(int num_regions)
{
	image_t *retval;
	int      alloc_size;

	alloc_size= sizeof(image_t)+(num_regions-1)*sizeof(elf_region_t);

	retval= rldalloc(alloc_size);

	memset(retval, 0, alloc_size);

	retval->num_regions= num_regions;

	return retval;
}

static
void
parse_program_headers(image_t *image, char *buff, int phnum, int phentsize)
{
	int i;
	int regcount;
	struct Elf32_Phdr *pheaders;

	regcount= 0;
	for(i= 0; i< phnum; i++) {
		pheaders= (struct Elf32_Phdr *)(buff+i*phentsize);

		switch(pheaders->p_type) {
			case PT_NULL:
				/* NOP header */
				break;
			case PT_LOAD:
				if(pheaders->p_memsz== pheaders->p_filesz) {
					/*
					 * everything in one area
					 */
					image->regions[regcount].start  = pheaders->p_vaddr;
					image->regions[regcount].size   = pheaders->p_memsz;
					image->regions[regcount].vmstart= ROUNDOWN(pheaders->p_vaddr, PAGE_SIZE);
					image->regions[regcount].vmsize = ROUNDUP (pheaders->p_memsz + (pheaders->p_vaddr % PAGE_SIZE), PAGE_SIZE);
					image->regions[regcount].fdstart= pheaders->p_offset;
					image->regions[regcount].fdsize = pheaders->p_filesz;
					image->regions[regcount].delta= 0;
					image->regions[regcount].flags= 0;
					if(pheaders->p_flags & PF_W) {
						// this is a writable segment
						image->regions[regcount].flags|= RFLAG_RW;
					}
				} else {
					/*
					 * may require splitting
					 */
					unsigned A= pheaders->p_vaddr+pheaders->p_memsz;
					unsigned B= pheaders->p_vaddr+pheaders->p_filesz;

					A= PAGE_BASE(A);
					B= PAGE_BASE(B);

					image->regions[regcount].start  = pheaders->p_vaddr;
					image->regions[regcount].size   = pheaders->p_filesz;
					image->regions[regcount].vmstart= ROUNDOWN(pheaders->p_vaddr, PAGE_SIZE);
					image->regions[regcount].vmsize = ROUNDUP (pheaders->p_filesz + (pheaders->p_vaddr % PAGE_SIZE), PAGE_SIZE);
					image->regions[regcount].fdstart= pheaders->p_offset;
					image->regions[regcount].fdsize = pheaders->p_filesz;
					image->regions[regcount].delta= 0;
					image->regions[regcount].flags= 0;
					if(pheaders->p_flags & PF_W) {
						// this is a writable segment
						image->regions[regcount].flags|= RFLAG_RW;
					}

					if(A!= B) {
						/*
						 * yeah, it requires splitting
						 */
						regcount+= 1;
						image->regions[regcount].start  = pheaders->p_vaddr;
						image->regions[regcount].size   = pheaders->p_memsz - pheaders->p_filesz;
						image->regions[regcount].vmstart= image->regions[regcount-1].vmstart + image->regions[regcount-1].vmsize;
						image->regions[regcount].vmsize = ROUNDUP (pheaders->p_memsz + (pheaders->p_vaddr % PAGE_SIZE), PAGE_SIZE) - image->regions[regcount-1].vmsize;
						image->regions[regcount].fdstart= 0;
						image->regions[regcount].fdsize = 0;
						image->regions[regcount].delta= 0;
						image->regions[regcount].flags= RFLAG_ANON;
						if(pheaders->p_flags & PF_W) {
							// this is a writable segment
							image->regions[regcount].flags|= RFLAG_RW;
						}
					}
				}
				regcount+= 1;
				break;
			case PT_DYNAMIC:
				image->dynamic_ptr = pheaders->p_vaddr;
				break;
			case PT_INTERP:
				/* should check here for appropiate interpreter */
				break;
			case PT_NOTE:
				/* unsupported */
				break;
			case PT_SHLIB:
				/* undefined semantics */
				break;
			case PT_PHDR:
				/* we don't use it */
				break;
			default:
				FATAL(true, "unhandled pheader type 0x%x\n", pheaders[i].p_type);
				break;
		}
	}
}

static
bool
assert_dynamic_loadable(image_t *image)
{
	unsigned i;

	if(!image->dynamic_ptr) {
		return true;
	}

	for(i= 0; i< image->num_regions; i++) {
		if(image->dynamic_ptr>= image->regions[i].start) {
			if(image->dynamic_ptr< image->regions[i].start+image->regions[i].size) {
				return true;
			}
		}
	}

	return false;
}

static
bool
load_image(int fd, char const *path, image_t *image, bool fixed)
{
	unsigned i;

	(void)(fd);

	for(i= 0; i< image->num_regions; i++) {
		char     region_name[256];
		addr     load_address;
		unsigned addr_specifier;

		sprintf(
			region_name,
			"%s:seg_%d(%s)",
			path,
			i,
			(image->regions[i].flags&RFLAG_RW)?"RW":"RO"
		);

		if(image->dynamic_ptr && !fixed) {
			/*
			 * relocatable image... we can afford to place wherever
			 */
			if(i== 0) {
				/*
				 * but only the first segment gets a free ride
				 */
				load_address= 0;
				addr_specifier= REGION_ADDR_ANY_ADDRESS;
			} else {
				load_address= image->regions[i].vmstart + image->regions[i-1].delta;
				addr_specifier= REGION_ADDR_EXACT_ADDRESS;
			}
		} else {
			/*
			 * not relocatable, put it where it asks or die trying
			 */
			load_address= image->regions[i].vmstart;
			addr_specifier= REGION_ADDR_EXACT_ADDRESS;
		}

		if(image->regions[i].flags & RFLAG_ANON) {
			image->regions[i].id= sys_vm_create_anonymous_region(
				region_name,
				(void **)&load_address,
				addr_specifier,
				image->regions[i].vmsize,
				REGION_WIRING_LAZY,
				LOCK_RW
			);

			if(image->regions[i].id < 0) {
				goto error;
			}
			image->regions[i].delta  = load_address - image->regions[i].vmstart;
			image->regions[i].vmstart= load_address;
		} else {
			image->regions[i].id= sys_vm_map_file(
				region_name,
				(void **)&load_address,
				addr_specifier,
				image->regions[i].vmsize,
				LOCK_RW,
				REGION_PRIVATE_MAP,
				path,
				ROUNDOWN(image->regions[i].fdstart, PAGE_SIZE)
			);
			if(image->regions[i].id < 0) {
				goto error;
			}
			image->regions[i].delta  = load_address - image->regions[i].vmstart;
			image->regions[i].vmstart= load_address;

			/*
			 * handle trailer bits in data segment
			 */
			if(image->regions[i].flags & RFLAG_RW) {
				unsigned start_clearing;
				unsigned to_clear;

				start_clearing=
					image->regions[i].vmstart
					+ PAGE_OFFS(image->regions[i].start)
					+ image->regions[i].size;
				to_clear=
					image->regions[i].vmsize
					- PAGE_OFFS(image->regions[i].start)
					- image->regions[i].size;
				memset((void*)start_clearing, 0, to_clear);
			}
		}
	}

	if(image->dynamic_ptr) {
		image->dynamic_ptr+= image->regions[0].delta;
	}

	return true;

error:
	return false;
}

static
bool
parse_dynamic_segment(image_t *image)
{
	struct Elf32_Dyn *d;
	int i;
//	int needed_offset = -1;

	image->symhash = 0;
	image->syms = 0;
	image->strtab = 0;

	d = (struct Elf32_Dyn *)image->dynamic_ptr;
	if(!d) {
		return true;
	}

	for(i=0; d[i].d_tag != DT_NULL; i++) {
		switch(d[i].d_tag) {
			case DT_NEEDED:
				image->num_needed+= 1;
				break;
			case DT_HASH:
				image->symhash = (unsigned int *)(d[i].d_un.d_ptr + image->regions[0].delta);
				break;
			case DT_STRTAB:
				image->strtab = (char *)(d[i].d_un.d_ptr + image->regions[0].delta);
				break;
			case DT_SYMTAB:
				image->syms = (struct Elf32_Sym *)(d[i].d_un.d_ptr + image->regions[0].delta);
				break;
			case DT_REL:
				image->rel = (struct Elf32_Rel *)(d[i].d_un.d_ptr + image->regions[0].delta);
				break;
			case DT_RELSZ:
				image->rel_len = d[i].d_un.d_val;
				break;
			case DT_RELA:
				image->rela = (struct Elf32_Rela *)(d[i].d_un.d_ptr + image->regions[0].delta);
				break;
			case DT_RELASZ:
				image->rela_len = d[i].d_un.d_val;
				break;
			// TK: procedure linkage table
			case DT_JMPREL:
				image->pltrel = (struct Elf32_Rel *)(d[i].d_un.d_ptr + image->regions[0].delta);
				break;
			case DT_PLTRELSZ:
				image->pltrel_len = d[i].d_un.d_val;
				break;
			default:
				continue;
		}
	}

	// lets make sure we found all the required sections
	if(!image->symhash || !image->syms || !image->strtab) {
		return false;
	}

	return true;
}

static
struct
Elf32_Sym *find_symbol(image_t **shimg, const char *name)
{
	image_t *iter;
	unsigned int hash;
	unsigned int i;

	iter= loaded_images.head;
	while(iter) {
		if(iter->dynamic_ptr) {
			hash = elf_hash(name) % HASHTABSIZE(iter);
			for(i = HASHBUCKETS(iter)[hash]; i != STN_UNDEF; i = HASHCHAINS(iter)[i]) {
				if(iter->syms[i].st_shndx!= SHN_UNDEF) {
					if((ELF32_ST_BIND(iter->syms[i].st_info)== STB_GLOBAL) || (ELF32_ST_BIND(iter->syms[i].st_info)== STB_WEAK)) {
						if(!strcmp(SYMNAME(iter, &iter->syms[i]), name)) {
							*shimg= iter;
							return &iter->syms[i];
						}
					}
				}
			}
		}

		iter= iter->next;
	}

	return NULL;
}

static
int
resolve_symbol(image_t *image, struct Elf32_Sym *sym, addr *sym_addr)
{
	struct Elf32_Sym *sym2;
	char             *symname;
	image_t          *shimg;

	switch(sym->st_shndx) {
		case SHN_UNDEF:
			// patch the symbol name
			symname= SYMNAME(image, sym);

			// it's undefined, must be outside this image, try the other image
			sym2 = find_symbol(&shimg, symname);
			if(!sym2) {
				printf("elf_resolve_symbol: could not resolve symbol '%s'\n", symname);
				return ERR_ELF_RESOLVING_SYMBOL;
			}

			// make sure they're the same type
			if(ELF32_ST_TYPE(sym->st_info)!= STT_NOTYPE) {
				if(ELF32_ST_TYPE(sym->st_info) != ELF32_ST_TYPE(sym2->st_info)) {
					printf("elf_resolve_symbol: found symbol '%s' in shared image but wrong type\n", symname);
					return ERR_ELF_RESOLVING_SYMBOL;
				}
			}

			if(ELF32_ST_BIND(sym2->st_info) != STB_GLOBAL && ELF32_ST_BIND(sym2->st_info) != STB_WEAK) {
				printf("elf_resolve_symbol: found symbol '%s' but not exported\n", symname);
				return ERR_ELF_RESOLVING_SYMBOL;
			}

			*sym_addr = sym2->st_value + shimg->regions[0].delta;
			return NO_ERROR;
		case SHN_ABS:
			*sym_addr = sym->st_value + image->regions[0].delta;
			return NO_ERROR;
		case SHN_COMMON:
			// XXX finish this
			printf("elf_resolve_symbol: COMMON symbol, finish me!\n");
			return ERR_NOT_IMPLEMENTED_YET;
		default:
			// standard symbol
			*sym_addr = sym->st_value + image->regions[0].delta;
			return NO_ERROR;
	}
}

static
int
relocate_rel(image_t *image, struct Elf32_Rel *rel, int rel_len )
{
	int i;
	struct Elf32_Sym *sym;
	int vlErr;
	addr S;
	addr final_val;

# define P         ((addr *)(image->regions[0].delta + rel[i].r_offset))
# define A         (*(P))
# define B         (image->regions[0].delta)

	for(i = 0; i * (int)sizeof(struct Elf32_Rel) < rel_len; i++) {

		unsigned type= ELF32_R_TYPE(rel[i].r_info);

		switch(ELF32_R_TYPE(rel[i].r_info)) {
			case R_386_32:
			case R_386_PC32:
			case R_386_GLOB_DAT:
			case R_386_JMP_SLOT:
			case R_386_GOTOFF:
				sym = SYMBOL(image, ELF32_R_SYM(rel[i].r_info));

				vlErr = resolve_symbol(image, sym, &S);
				if(vlErr<0) {
					return vlErr;
				}
		}
		switch(type) {
			case R_386_NONE:
				continue;
			case R_386_32:
				final_val= S+A;
				break;
			case R_386_PC32:
				final_val=S+A-(addr)P;
				break;
#if 0
			case R_386_GOT32:
				final_val= G+A;
				break;
			case R_386_PLT32:
				final_val= L+A-(addr)P;
				break;
#endif
			case R_386_COPY:
				/* what ? */
				continue;
			case R_386_GLOB_DAT:
				final_val= S;
				break;
			case R_386_JMP_SLOT:
				final_val= S;
				break;
			case R_386_RELATIVE:
				final_val= B+A;
				break;
#if 0
			case R_386_GOTOFF:
				final_val= S+A-GOT;
				break;
			case R_386_GOTPC:
				final_val= GOT+A-P;
				break;
#endif
			default:
				printf("unhandled relocation type %d\n", ELF32_R_TYPE(rel[i].r_info));
				return ERR_NOT_ALLOWED;
		}

		*P= final_val;
	}

# undef P
# undef A
# undef B


	return NO_ERROR;
}

static
bool
relocate_image(image_t *image)
{
	int res = NO_ERROR;
	int i;

	// deal with the rels first
	if(image->rel) {
		res= relocate_rel( image, image->rel, image->rel_len );

		if(res) {
			return false;
		}
	}

	if(image->pltrel) {
		res= relocate_rel(image, image->pltrel, image->pltrel_len);

		if(res) {
			return false;
		}
	}

	if(image->rela) {
		printf("RELA relocations not supported\n");
		return ERR_NOT_ALLOWED;
		for(i = 1; i * (int)sizeof(struct Elf32_Rela) < image->rela_len; i++) {
			printf("rela: type %d\n", ELF32_R_TYPE(image->rela[i].r_info));
		}
	}

	return true;
}


static
image_t *
load_container(char const *path, bool fixed)
{
	int      fd;
	int      len;
	int      ph_len;
	char     ph_buff[4096];
	int      num_regions;
	bool     load_success;
	bool     dynamic_success;
//	bool     relocate_success;
	image_t *image;

	struct Elf32_Ehdr eheader;

	fd= sys_open(path, STREAM_TYPE_FILE, 0);
	FATAL((fd< 0), "cannot open file %s\n", path);

	len= sys_read(fd, &eheader, 0, sizeof(eheader));
	FATAL((len!= sizeof(eheader)), "troubles reading ELF header\n");

	ph_len= parse_eheader(&eheader);
	FATAL((ph_len<= 0), "incorrect ELF header\n");
	FATAL((ph_len> (int)sizeof(ph_buff)), "cannot handle Program headers bigger than %lu\n", (long unsigned)sizeof(ph_buff));

	len= sys_read(fd, ph_buff, eheader.e_phoff, ph_len);
	FATAL((len!= ph_len), "troubles reading Program headers\n");

	num_regions= count_regions(ph_buff, eheader.e_phnum, eheader.e_phentsize);
	FATAL((num_regions<= 0), "troubles parsing Program headers, num_regions= %d\n", num_regions);

	image= create_image(num_regions);
	FATAL((!image), "failed to allocate image_t control block\n");

	parse_program_headers(image, ph_buff, eheader.e_phnum, eheader.e_phentsize);
	FATAL(!assert_dynamic_loadable(image), "dynamic segment must be loadable (implementation restriction)\n");

	load_success= load_image(fd, path, image, fixed);
	FATAL(!load_success, "troubles reading image\n");

	dynamic_success= parse_dynamic_segment(image);
	FATAL(!dynamic_success, "troubles handling dynamic section\n");

//	relocate_success= relocate_image(image);
//	FATAL(!relocate_success, "troubles relocating\n");

	image->entry_point= eheader.e_entry + image->regions[0].delta;

	sys_close(fd);

	enqueue(image);

	return image;
}


static
void
load_dependencies(image_t *img)
{
	unsigned i;

	struct Elf32_Dyn *d;
	addr   needed_offset;
	char   path[256];

	d = (struct Elf32_Dyn *)img->dynamic_ptr;
	if(!d) {
		return;
	}

	for(i=0; d[i].d_tag != DT_NULL; i++) {
		switch(d[i].d_tag) {
			case DT_NEEDED:
				needed_offset = d[i].d_un.d_ptr;
				sprintf(path, "/boot/lib/%s", STRING(img, needed_offset));
				load_container(path, false);

				break;
			default:
				/*
				 * ignore any other tag
				 */
				continue;
		}
	}

	return;
}

int
load_program(char const *path, void **entry)
{
	image_t *image;
	image_t *iter;


	image = load_container(path, true);

	iter= loaded_images.head;
	while(iter) {
		load_dependencies(iter);

		iter= iter->next;
	};

	iter= loaded_images.head;
	while(iter) {
		bool relocate_success;

		relocate_success= relocate_image(iter);
		FATAL(!relocate_success, "troubles relocating\n");

		iter= iter->next;
	};

	*entry= (void*)(image->entry_point);
	return 0;
}
