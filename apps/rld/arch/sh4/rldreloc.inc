/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


/*
 * rldelf.c requires this function to be implemented on a per-cpu basis
 */
static
int
relocate_rel(image_t *image, struct Elf32_Rel *rel, int rel_len )
{
	int i;
	struct Elf32_Sym *sym;
	int vlErr;
	addr_t S;
	addr_t final_val;

#define P         ((addr_t *)(image->regions[0].delta + rel[i].r_offset))
#define A         (*(P))
#define B         (image->regions[0].delta)

	printf("rel_len %d\n", rel_len);

	for(i = 0; i * (int)sizeof(struct Elf32_Rel) < rel_len; i++) {
		unsigned type = ELF32_R_TYPE(rel[i].r_info);

		switch(type) {
			case R_SH_DIR32:
			case R_SH_JMP_SLOT:
				sym = SYMBOL(image, ELF32_R_SYM(rel[i].r_info));

				vlErr = resolve_symbol(image, sym, &S);
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
				final_val=S+A-(addr_t)P;
				break;
#if 0
			case R_386_GOT32:
				final_val= G+A;
				break;
			case R_386_PLT32:
				final_val= L+A-(addr_t)P;
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
				printf("unhandled Rel Entry @ %p: offset 0x%x, info 0x%x\n", &rel[i], rel[i].r_offset, rel[i].r_info);
				printf("type %d at %p\n", type, P);
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

static
int
relocate_rela(image_t *image, struct Elf32_Rela *rel, int rel_len )
{
	int i;
	struct Elf32_Sym *sym;
	int vlErr;
	addr_t S;
	addr_t final_val;

#define P         ((addr_t *)(image->regions[0].delta + rel[i].r_offset))
#define A         ((addr_t)rel[i].r_addend)
#define B         (image->regions[0].delta)

	printf("rela_len %d\n", rel_len);

	for(i = 0; i * (int)sizeof(struct Elf32_Rela) < rel_len; i++) {
		unsigned type = ELF32_R_TYPE(rel[i].r_info);

		switch(type) {
			case R_SH_DIR32:
			case R_SH_JMP_SLOT:
			case R_SH_GLOB_DAT:
				sym = SYMBOL(image, ELF32_R_SYM(rel[i].r_info));

				vlErr = resolve_symbol(image, sym, &S);
				if(vlErr<0) {
					return vlErr;
				}
		}
		switch(type) {
			case R_SH_NONE:
				continue;
			case R_SH_DIR32:
//				printf("DIR32: @ %p B 0x%x, S 0x%x, A 0x%x\n", P, B, S, A);
				final_val = S + A;
				break;
			case R_SH_GLOB_DAT:
//				printf("GLOB_DAT: @ %p S 0x%x, A 0x%x\n", P, S, A);
				final_val = S + A;
				break;
			case R_SH_JMP_SLOT:
//				printf("JMP_SLOT: @ %p S 0x%x, A 0x%x\n", P, S, A);
				final_val = S + A;
				break;
			case R_SH_REL32:
//				printf("REL32: @ %p S 0x%x, A 0x%x, P 0x%x\n", P, S, A, P);
				final_val = S + A - (addr_t)P;
				break;
			case R_SH_RELATIVE:
//				printf("RELATIVE: @ %p B 0x%x A 0x%x *P 0x%x\n", P, B, A, *P);
					final_val = B + A;
				break;
			default:
//				printf("unhandled Rela Entry @ %p: offset 0x%x, info 0x%x, addend 0x%x\n", &rel[i], rel[i].r_offset, rel[i].r_info, rel[i].r_addend);
//				printf("type %d at %p\n", type, P);
				continue;
		}

//		printf("putting 0x%x into %p\n", final_val, P);
		*P = final_val;
	}

#undef P
#undef A
#undef B

	return NO_ERROR;
}

/*
 * rldelf.c requires this function to be implemented on a per-cpu basis
 */
static
bool
relocate_image(image_t *image)
{
	int res = NO_ERROR;
	int i;

	if(image->flags & RFLAG_RELOCATED) {
		return true;
	}
	image->flags|= RFLAG_RELOCATED;

	// deal with the rels first
	if(image->rel) {
		res = relocate_rel(image, image->rel, image->rel_len);
		if(res) {
			return false;
		}
	}

	if(image->pltrel) {
		if(image->pltrel_type == DT_REL)
			res = relocate_rel(image, image->pltrel, image->pltrel_len);
		else
			res = relocate_rela(image, (struct Elf32_Rela *)image->pltrel, image->pltrel_len);
		if(res) {
			return false;
		}
	}

	if(image->rela) {
		res = relocate_rela(image, image->rela, image->rela_len);
		if(res) {
			return false;
		}
	}

	return true;
}

