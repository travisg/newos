#ifndef _ARCH_STAGE2_H
#define _ARCH_STAGE2_H

#ifdef ARCH_i386
#include <boot/arch/i386/stage2.h>
#endif
#ifdef ARCH_sh4
#include <boot/arch/sh4/stage2.h>
#endif
#ifdef ARCH_sparc64
#include <boot/arch/sparc64/stage2.h>
#endif

#endif
