/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/dev/devs.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>

#include <string.h>

#ifdef ARCH_i386
#include <kernel/dev/arch/i386/ide/ide_bus.h>
#include <kernel/dev/arch/i386/console/console_dev.h>
#include <kernel/dev/arch/i386/keyboard/keyboard.h>
#include <kernel/dev/arch/i386/ps2mouse/ps2mouse.h>
#include <kernel/dev/arch/i386/rtl8139/rtl8139_dev.h>
#endif
#ifdef ARCH_sh4
#include <kernel/dev/arch/sh4/maple/maple_bus.h>
#include <kernel/dev/arch/i386/keyboard/keyboard.h>
#include <kernel/dev/arch/sh4/console/console_dev.h>
#include <kernel/dev/arch/sh4/rtl8139/rtl8139_dev.h>
#endif
#include <kernel/dev/common/null.h>
#include <kernel/dev/common/zero.h>
#include <kernel/dev/fb_console/fb_console.h>

/* loads all the fixed drivers */
int fixed_devs_init(kernel_args *ka)
{
	null_dev_init(ka);
	zero_dev_init(ka);
#ifdef ARCH_i386
//	ide_bus_init(ka);
	keyboard_dev_init(ka);
//	mouse_dev_init(ka);
	console_dev_init(ka);
	rtl8139_dev_init(ka);
#endif

#ifdef ARCH_sh4
	maple_bus_init(ka);
	keyboard_dev_init(ka);
//	console_dev_init(ka);
	rtl8139_dev_init(ka);
#endif
	fb_console_dev_init(ka);

	return 0;
}
