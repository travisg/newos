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
	int i;
	struct Elf32_Sym *sym;
	int vlErr;
	addr S;
	addr final_val;

#define P         ((addr *)(image->regions[0].delta + rel[i].r_offset))
#define A         (*(P))
#define B         (image->regions[0].delta)

	dprintf("rel_len %d\n", rel_len);

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

#if 0
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
#endif
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
#if 0
			case R_386_NONE:
				continue;
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
#endif
			default:
				dprintf("unhandled Rel Entry @ %p: offset 0x%x, info 0x%x\n", &rel[i], rel[i].r_offset, rel[i].r_info);
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
	addr S;
	addr final_val;

#define P         ((addr *)(image->regions[0].delta + rel[i].r_offset))
#define A         ((addr)rel[i].r_addend)
#define B         (image->regions[0].delta)

	dprintf("rela_len %d\n", rel_len);

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
				dprintf("DIR32: @ %p B 0x%x, S 0x%x, A 0x%x\n", P, B, S, A);
				final_val = /*B + */ S + A;
				break;
			case R_SH_JMP_SLOT:
				dprintf("JMP_SLOT: @ %p S 0x%x, A 0x%x\n", P, S, A);
				final_val = S + A;
				break;
			case R_SH_RELATIVE:
				dprintf("RELATIVE: @ %p B 0x%x A 0x%x *P 0x%x\n", P, B, A, *P);
				if(A)
					final_val = B + A;
				else
					final_val = B + *P;
				break;
			default:
				dprintf("unhandled Rela Entry @ %p: offset 0x%x, info 0x%x, addend 0x%x\n", &rel[i], rel[i].r_offset, rel[i].r_info, rel[i].r_addend);
				dprintf("type %d at %p\n", type, P);
				continue;
		}

		dprintf("putting 0x%x into %p\n", final_val, P);
		*P = final_val;
	}

#undef P
#undef A
#undef B

	return NO_ERROR;
}

