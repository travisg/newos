#ifndef _ARCH_KTYPES_H
#define _ARCH_KTYPES_H

#ifdef ARCH_i386
#include <kernel/arch/i386/ktypes.h>
#endif
#ifdef ARCH_sh4
#include <kernel/arch/sh4/ktypes.h>
#endif
#ifdef ARCH_alpha
#include <kernel/arch/alpha/ktypes.h>
#endif
#ifdef ARCH_sparc64
#include <kernel/arch/sparc64/ktypes.h>
#endif

#endif

