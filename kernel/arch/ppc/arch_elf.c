/*
** Copyright 2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/elf.h>
#include <newos/elf32.h>
#include <kernel/arch/elf.h>

int arch_elf_relocate_rel(struct elf_image_info *image, const char *sym_prepend,
	struct elf_image_info *resolve_image, struct Elf32_Rel *rel, int rel_len)
{
	dprintf("arch_elf_relocate_rel: not supported on ppc\n");

	return ERR_NOT_IMPLEMENTED;
}

int arch_elf_relocate_rela(struct elf_image_info *image, const char *sym_prepend,
	struct elf_image_info *resolve_image, struct Elf32_Rela *rel, int rel_len)
{
	dprintf("arch_elf_relocate_rela: not supported on ppc\n");
	return ERR_NOT_IMPLEMENTED;
}

