/*
** Copyright 2002-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/elf.h>
#include <kernel/debug.h>
#include <newos/elf32.h>
#include <kernel/arch/elf.h>

#define CHATTY_REL 0

#if CHATTY_REL
#define TRACE(x) dprintf x
#else
#define TRACE(x)
#endif

int arch_elf_relocate_rel(struct elf_image_info *image, const char *sym_prepend,
	struct elf_image_info *resolve_image, struct Elf32_Rel *rel, int rel_len)
{
	int i;
	struct Elf32_Sym *sym;
	int vlErr;
	addr_t S;
	addr_t final_val;

#define P         ((addr_t *)(image->regions[0].delta + rel[i].r_offset))
#define A         (*(P))
#define B         (image->regions[0].delta)

	TRACE(("rel_len %d\n", rel_len));

	for(i = 0; i * (int)sizeof(struct Elf32_Rel) < rel_len; i++) {
		unsigned type = ELF32_R_TYPE(rel[i].r_info);

		switch(type) {
			case R_SH_DIR32:
			case R_SH_JMP_SLOT:
				sym = SYMBOL(image, ELF32_R_SYM(rel[i].r_info));

				vlErr = elf_resolve_symbol(image, sym, resolve_image, sym_prepend, &S);
				if(vlErr<0) {
					return vlErr;
				}
				break;
		}
		switch(type) {
			case R_SH_NONE:
				continue;
			case R_SH_DIR32:
				final_val= B + S + A;
				break;
			case R_SH_JMP_SLOT:
				final_val = S + A;
				break;
			case R_SH_RELATIVE:
				final_val= B+A;
				break;
			default:
				dprintf("arch_elf_relocate_rel: unhandled Rel Entry @ %p: offset 0x%x, info 0x%x\n", &rel[i], rel[i].r_offset, rel[i].r_info);
				dprintf("type %d at %p\n", type, P);
				continue;
				//return ERR_NOT_ALLOWED;
		}

		*P = final_val;
	}

#undef P
#undef A
#undef B

	return NO_ERROR;
}

int arch_elf_relocate_rela(struct elf_image_info *image, const char *sym_prepend,
	struct elf_image_info *resolve_image, struct Elf32_Rela *rel, int rel_len)
{
	int i;
	struct Elf32_Sym *sym;
	int vlErr;
	addr_t S;
	addr_t final_val;

#define P         ((addr_t *)(image->regions[0].delta + rel[i].r_offset))
#define A         ((addr_t)rel[i].r_addend)
#define B         (image->regions[0].delta)

	TRACE(("rela_len %d\n", rel_len));

	for(i = 0; i * (int)sizeof(struct Elf32_Rela) < rel_len; i++) {
		unsigned type = ELF32_R_TYPE(rel[i].r_info);

		switch(type) {
			case R_SH_DIR32:
			case R_SH_JMP_SLOT:
				sym = SYMBOL(image, ELF32_R_SYM(rel[i].r_info));

				vlErr = elf_resolve_symbol(image, sym, resolve_image, sym_prepend, &S);
				if(vlErr<0) {
					return vlErr;
				}
		}
		switch(type) {
			case R_SH_NONE:
				continue;
			case R_SH_DIR32:
				TRACE(("DIR32: @ %p S 0x%x, A 0x%x\n", P, S, A));
				final_val = S + A;
				break;
			case R_SH_JMP_SLOT:
				TRACE(("JMP_SLOT: @ %p S 0x%x, A 0x%x\n", P, S, A));
				final_val = S + A;
				break;
			case R_SH_RELATIVE:
				TRACE(("RELATIVE: @ %p B 0x%x A 0x%x *P 0x%x\n", P, B, A, *P));
				if(A)
					final_val = B + A;
				else
					final_val = B + *P;
				break;
			default:
				dprintf("arch_elf_relocate_rela: unhandled Rela Entry @ %p: offset 0x%x, info 0x%x, addend 0x%x\n", &rel[i], rel[i].r_offset, rel[i].r_info, rel[i].r_addend);
				dprintf("type %d at %p\n", type, P);
				continue;
		}

		TRACE(("putting 0x%x into %p\n", final_val, P));
		*P = final_val;
	}

#undef P
#undef A
#undef B

	return NO_ERROR;
}

