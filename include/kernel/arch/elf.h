/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_KERNEL_ARCH_ELF_H
#define _NEWOS_KERNEL_ARCH_ELF_H

int arch_elf_relocate_rel(struct elf_image_info *image, const char *sym_prepend,
	struct elf_image_info *resolve_image, struct Elf32_Rel *rel, int rel_len);
int arch_elf_relocate_rela(struct elf_image_info *image, const char *sym_prepend,
	struct elf_image_info *resolve_image, struct Elf32_Rela *rel, int rel_len);

#ifdef ARCH_i386
#include <kernel/arch/i386/elf.h>
#endif
#ifdef ARCH_sh4
#include <kernel/arch/sh4/elf.h>
#endif
#ifdef ARCH_alpha
#include <kernel/arch/alpha/elf.h>
#endif
#ifdef ARCH_sparc64
#include <kernel/arch/sparc64/elf.h>
#endif
#ifdef ARCH_mips
#include <kernel/arch/mips/elf.h>
#endif
#ifdef ARCH_ppc
#include <kernel/arch/ppc/elf.h>
#endif
#ifdef ARCH_m68k
#include <kernel/arch/m68k/elf.h>
#endif

#endif

