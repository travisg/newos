/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _ELF64_H
#define _ELF64_H

#include <newos/elf.h>

typedef uint64 Elf64_Addr;
typedef uint32 Elf64_Half;
typedef uint16 Elf64_Quarter;
typedef uint64 Elf64_Off;
typedef uint64 Elf64_Size;
typedef int64 Elf64_Sword;
typedef uint64 Elf64_Word;

struct Elf64_Ehdr {
	unsigned char	e_ident[EI_NIDENT];
	Elf64_Quarter	e_type;
	Elf64_Quarter	e_machine;
	Elf64_Half		e_version;
	Elf64_Addr		e_entry;
	Elf64_Off		e_phoff;
	Elf64_Off		e_shoff;
	Elf64_Half		e_flags;
	Elf64_Quarter	e_ehsize;
	Elf64_Quarter	e_phentsize;
	Elf64_Quarter	e_phnum;
	Elf64_Quarter	e_shentsize;
	Elf64_Quarter	e_shnum;
	Elf64_Quarter	e_shstrndx;
};

struct Elf64_Shdr {
	Elf64_Half		sh_name;
	Elf64_Half 		sh_type;
	Elf64_Size		sh_flags;
	Elf64_Addr		sh_addr;
	Elf64_Off		sh_offset;
	Elf64_Size		sh_size;
	Elf64_Half		sh_link;
	Elf64_Half		sh_info;
	Elf64_Size		sh_addralign;
	Elf64_Size		sh_entsize;
} ;

struct Elf64_Phdr {
	Elf64_Half		p_type;
	Elf64_Half		p_flags;
	Elf64_Off		p_offset;
	Elf64_Addr		p_vaddr;
	Elf64_Addr		p_paddr;
	Elf64_Size		p_filesz;
	Elf64_Size		p_memsz;
	Elf64_Size		p_align;
};

struct Elf64_Sym {
	Elf64_Half		st_name;
	unsigned char	st_info;
	unsigned char 	st_other;
	Elf64_Quarter	st_shndx;
	Elf64_Addr		st_value;
	Elf64_Size		st_size;
};

#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xf)
#define ELF64_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

struct Elf64_Rel {
	Elf64_Addr r_offset;
	Elf64_Size r_info;
};

struct Elf64_Rela {
	Elf64_Addr r_offset;
	Elf64_Size r_info;
	Elf64_Off  r_addend;
};

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s, t) (((s) << 32) + (unsigned char)(t))

struct Elf64_Dyn {
	Elf64_Size d_tag;
	union {
		Elf64_Size d_val;
		Elf64_Addr d_ptr;
	} d_un;
};

#endif
