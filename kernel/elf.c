#include <kernel.h>
#include <elf.h>
#include <vfs.h>
#include <thread.h>
#include <string.h>
#include <debug.h>
#include <printf.h>
#include <arch_cpu.h>

typedef uint32 Elf32_Addr;
typedef uint16 Elf32_Half;
typedef uint32 Elf32_Off;
typedef int32 Elf32_Sword;
typedef uint32 Elf32_Word;

#define ELF_MAGIC "\x7f""ELF"
#define EI_NIDENT 16

struct Elf32_Ehdr {
	unsigned char	e_ident[EI_NIDENT];
	Elf32_Half		e_type;
	Elf32_Half		e_machine;
	Elf32_Word		e_version;
	Elf32_Addr		e_entry;
	Elf32_Off		e_phoff;
	Elf32_Off		e_shoff;
	Elf32_Word		e_flags;
	Elf32_Half		e_ehsize;
	Elf32_Half		e_phentsize;
	Elf32_Half		e_phnum;
	Elf32_Half		e_shentsize;
	Elf32_Half		e_shnum;
	Elf32_Half		e_shstrndx;
} _PACKED;

#define ELFCLASS32 1
#define ELFDATA2LSB 1

struct Elf32_Shdr {
	Elf32_Word		sh_name;
	Elf32_Word		sh_type;
	Elf32_Word		sh_flags;
	Elf32_Addr		sh_addr;
	Elf32_Off		sh_offset;
	Elf32_Word		sh_size;
	Elf32_Word		sh_link;
	Elf32_Word		sh_info;
	Elf32_Word		sh_addralign;
	Elf32_Word		sh_entsize;
} _PACKED;

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7fffffff
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0xffffffff

#define SHF_WRITE 1

struct Elf32_Phdr {
	Elf32_Word		p_type;
	Elf32_Off		p_offset;
	Elf32_Addr		p_vaddr;
	Elf32_Addr		p_paddr;
	Elf32_Word		p_filesz;
	Elf32_Word		p_memsz;
	Elf32_Word		p_flags;
	Elf32_Word		p_align;
} _PACKED;

#define PF_X		0x1
#define PF_W		0x2
#define PF_R		0x4
#define PF_MASKPROC	0xf0000000


#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7fffffff

struct Elf32_Sym {
	Elf32_Word		st_name;
	Elf32_Addr		st_value;
	Elf32_Word		st_size;
	unsigned char	st_info;
	unsigned char 	st_other;
	Elf32_Half		st_shndx;
} _PACKED;

#define ELF32_ST_BIND(i) ((i) >> 4)
#define ELF32_ST_TYPE(i) ((i) & 0xf)
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_LOPROC 13
#define STT_HIPROC 15

struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;
}  _PACKED Elf32_Rel;

struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;
	Elf32_Sword r_addend;
}  _PACKED Elf32_Rela;

#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 8) + (unsigned char)(t))

static int verify_eheader(struct Elf32_Ehdr *eheader)
{
	if(memcmp(eheader->e_ident, ELF_MAGIC, 4) != 0)
		return -1;

	if(eheader->e_ident[4] != ELFCLASS32)
		return -1;
		
	if(eheader->e_phoff == 0)
		return -1;
		
	if(eheader->e_phentsize < sizeof(struct Elf32_Phdr))
		return -1;
		
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
	
	fd = vfs_open(NULL, path, "", STREAM_TYPE_FILE);
	if(fd < 0)
		return fd;

	len = sizeof(eheader);
	err = vfs_read(fd, &eheader, 0, &len);
	if(err < 0)
		goto error;
	if(len != sizeof(eheader)) {
		// short read
		err = -1;
		goto error;
	}
	if(verify_eheader(&eheader) != 0) {
		err = -1;
		goto error;
	}
	
	pheaders = kmalloc(eheader.e_phnum * eheader.e_phentsize);
	if(pheaders == NULL) {
		dprintf("error allocating space for program headers\n");
		err = -1;
		goto error;
	}
	
	len = eheader.e_phnum * eheader.e_phentsize;
	dprintf("reading in program headers at 0x%x, len 0x%x\n", eheader.e_phoff, len);
	err = vfs_read(fd, pheaders, eheader.e_phoff, &len);
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
		char area_name[64];
		area_id a;
		char *area_addr;
		
		sprintf(area_name, "%s_seg%d", path, i);
		
		area_addr = (char *)pheaders[i].p_vaddr;
		a = vm_create_area(p->aspace, area_name, (void **)&area_addr, AREA_EXACT_ADDRESS,
			ROUNDUP(pheaders[i].p_memsz, PAGE_SIZE), LOCK_RW);
		if(a == NULL) {
			dprintf("error allocating area!\n");
			err = -1;
			goto error;
		}
		
		if(i == 0)
			*entry = pheaders[i].p_vaddr;
	}

	dprintf("elf_load: done!\n");

	err = 0;

error:
	vfs_close(fd);

	return err;
}
