/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _NEWOS_SYSCONFIG_H
#define _NEWOS_SYSCONFIG_H

/* system configuration, edit these to configure the build */

#if _KERNEL

/* set the debug level of the kernel */
/* debug level 0 prints no messages, 
 * >= 1 turns on a lot of debug spew and extra internal consistency checks */
#define DEBUG 10

/* set the ASSERT level of the kernel:
 * 0 = no asserts
 * 1 = critical asserts only
 * 2 = general assert level
 */
#define _ASSERT_LEVEL 2

/* sets if the kernel boots with serial spew on or not */
#define _DEFAULT_SERIAL_DBG_ON 1

/* set the com port of the serial debugger */
/* NOTE: a setting of 0xe9 enables the Bochs E9 hack */
#define _SERIAL_DBG_PORT 1 // com1 

/* set the size of the bootup kprintf buffer */
#define _BOOT_KPRINTF_BUF_SIZE 4096

/* compile in support for SMP or not */
#define _WITH_SMP 1

/* maximum number of supported cpus */
#define _MAX_CPUS 4

#if !_WITH_SMP
#undef _MAX_CPUS
#define _MAX_CPUS 1
#endif

#if ARCH_sh4
/* sh-4 will not support smp, ever */
#undef _WITH_SMP
#define _WITH_SMP 0
#undef _MAX_CPUS
#define _MAX_CPUS 1
#endif

#else

#endif

#endif
