#ifndef _ARCH_THREAD_STRUCT_H
#define _ARCH_THREAD_STRUCT_H

#ifdef ARCH_i386
#include <kernel/arch/i386/thread_struct.h>
#endif
#ifdef ARCH_sh4
#include <kernel/arch/sh4/thread_struct.h>
#endif
#ifdef ARCH_alpha
#include <kernel/arch/alpha/thread_struct.h>
#endif

#endif

