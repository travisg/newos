/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <dev/devs.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>

#include <libc/string.h>

#ifdef ARCH_i386
#include <dev/arch/i386/ide/ide_bus.h>
#include <dev/arch/i386/console/console_dev.h>
#include <dev/arch/i386/keyboard/keyboard.h>
#include <dev/arch/i386/rtl8139/rtl8139_dev.h>
#endif
#ifdef ARCH_sh4
#include <dev/arch/sh4/maple/maple_bus.h>
#include <dev/arch/sh4/console/console_dev.h>
#include <dev/arch/sh4/rtl8139/rtl8139_dev.h>
#endif
#include <dev/common/null.h>
#include <dev/common/zero.h>

int devs_init(kernel_args *ka)
{

	null_dev_init(ka);
	zero_dev_init(ka);
#ifdef ARCH_i386
//	ide_bus_init(ka);
	keyboard_dev_init(ka);
	console_dev_init(ka);
	rtl8139_dev_init(ka);
#endif	

#ifdef ARCH_sh4
	maple_bus_init(ka);
	console_dev_init(ka);
	rtl8139_dev_init(ka);
#endif

	return 0;
}
