/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <boot/stage2.h>
#include <boot/bootdir.h>
#include <arch/cpu.h>
#include <arch/arm/omap.h>
#include "stage1.h"
#include "inflate.h"

#define SRAM_BASE 0x20000000
#define SRAM_SIZE (148*1024)
#define SDRAM_BASE 0x10000000
#define SDRAM_SIZE (32*1024*1024)

extern void *_end;

static unsigned char *heap_ptr = (unsigned char *)&_end;
#define SRC ((void *)(((addr_t)&_end) - 0x20001000 - 0x74))
#define TARGET ((void *)0x10000000)

static void omap_bringup(void);
static void spin(int usecs);
void LED_DISPLAY_NUM(int n, bool loop);

void _decompress_start(void);
void _decompress_start(void)
{
	unsigned long len;
	boot_dir *bootdir = TARGET;
	void (*stage2_entry)(void);

	LED_DISPLAY(0, '2');

	omap_bringup();

	LED_DISPLAY(0, '3');

	write_cp15_control_reg(0x1106); // i-cache, d-cache, alignment fault, system protection

	LED_DISPLAY(0, '4');
	
	len = gunzip((unsigned char const *)SRC, TARGET, kmalloc(32*1024));

	LED_DISPLAY(0, '5');
	
//	dprintf("finding stage2...");
	stage2_entry = (void*)((char *)bootdir + bootdir->bd_entry[1].be_offset * PAGE_SIZE + bootdir->bd_entry[1].be_code_ventr);
//	dprintf("entry at %p\n", stage2_entry);

	LED_DISPLAY(0, '6');
	LED_DISPLAY_NUM((addr_t)stage2_entry, false);

	// jump into stage2
	stage2_entry();
}

/* set up any mux pin assignments needed to get this platform to work */
int platform_mux_init(void)
{
	*REG(FUNC_MUX_CTRL_0) = 0x00000000;
	*REG(FUNC_MUX_CTRL_1) = 0x00000000;
	*REG(FUNC_MUX_CTRL_2) = 0x00000000;
	*REG(FUNC_MUX_CTRL_3) = 0x00000000;
	*REG(FUNC_MUX_CTRL_4) = 0x00000000;
	*REG(FUNC_MUX_CTRL_5) = 0x00000000;

	// M18  -> TIMER.PWM0 (mode 5)
	// L14  -> TIMER.PWM1 (mode 4)
	// M20  -> TIMER.PWM2 (mode 2)
	// N19  -> KB.R[5] (mode 1)
	// N21  -> KB.R[6] (mode 1)
	*REG(FUNC_MUX_CTRL_6) = 0x00000000 | (5<<0) | (4<<3) | (2<<6) | (1<<9) | (1<<12);

	// R18  -> USB.VBUS (mode 2)
	// T20  -> LOW_PWR (mode 1)
	*REG(FUNC_MUX_CTRL_7) = 0x00000000 | (2<<9) | (1<<12);

	// U18  -> UART1.DSR (mode 2)
	// W21  -> UART1.DTR (mode 2)
	// V19  -> KB.C[7] (mode 1)
	// N14  -> UART3.TX (mode 4)
	// P15  -> UART3.RX (mode 4)
	*REG(FUNC_MUX_CTRL_8) = 0x00000000 | (2<<0) | (2<<3) | (1<<6) | (4<<9) | (4<<12);

	// Y14  -> UART1.TX (mode 1)
	// AA15 -> UART1.RTS (mode 1)
	// W16  -> GPIO42 (mode 7)
	// W15  -> GPIO40 (mode 7)
	*REG(FUNC_MUX_CTRL_9) = 0x00000000 | (1<<21) | (1<<12) | (7<<27) | (7<<3);

	// Y15  -> GPIO17 (mode 7)
    *REG(FUNC_MUX_CTRL_A) = 0x00000000 | (7<<0);

	// R10  -> GPIO23 (mode 7)
    *REG(FUNC_MUX_CTRL_B) = 0x00000000 | (7<<18);

	// V6  -> UART2.TX (mode 1)
	// W5  -> UART2.RTS (mode 1)
    *REG(FUNC_MUX_CTRL_C) = 0x00000000 | (1<<27) | (1<<24);

	// W4  -> USB.PUDIS (mode 3)
    *REG(FUNC_MUX_CTRL_D) = 0x00000018;
    *REG(FUNC_MUX_CTRL_E) = 0x00000000;
    *REG(FUNC_MUX_CTRL_F) = 0x00000000;

	// M7  -> FLASH.CS0 (mode 1)
    *REG(FUNC_MUX_CTRL_10) = 0x00000000 | (1<<0);
    *REG(FUNC_MUX_CTRL_11) = 0x00000000;
    *REG(FUNC_MUX_CTRL_12) = 0x00000000;

    *REG(PULL_DWN_CTRL_0) = 0x00000000;
    *REG(PULL_DWN_CTRL_1) = 0x00000000;
    *REG(PULL_DWN_CTRL_2) = 0x00000000;
    *REG(PULL_DWN_CTRL_3) = 0x00000000;
    *REG(PULL_DWN_CTRL_4) = 0x00000000;

    *REG(PU_PD_SEL_0) = 0x00000000;
    *REG(PU_PD_SEL_1) = 0x00000000;
    *REG(PU_PD_SEL_2) = 0x00000000;
    *REG(PU_PD_SEL_3) = 0x00000000;
    *REG(PU_PD_SEL_4) = 0x00000000;
    
	/* saw this configuration on the boot ROM */
//	*REG(PULL_DWN_CTRL_0) = 0;
//	*REG(PULL_DWN_CTRL_1) = 0xFFFEF7FF;
//	*REG(PULL_DWN_CTRL_2) = 0xD0282E59;
//	*REG(PULL_DWN_CTRL_3) = 0xFFFFFEFC;
//	*REG(PULL_DWN_CTRL_4) = 0x87FFFFF7;

	// usb port 0 as device, pulldown of DM/DP disabled
	*REG(USB_TRANSCEIVER_CTRL) = 6;

    // bit 17, VBUS detection controlled by DVDD2
    *REG(MOD_CONF_CTRL_0) |= 0x00020000;

    *REG(VOLTAGE_CTRL_0) = 0;

	// Turn on UART1/2/3 48 MHZ clock
    *REG(MOD_CONF_CTRL_0) |= 0xe0000000;

    *REG(COMP_MODE_CTRL_0) = 0x0000eaef;	// COMP_MODE_ENABLE

	return 0;
}

static void omap_bringup(void)
{
/* our external clock is 13Mhz */
	*REG(ULPD_PLL_CTRL_STATUS) = 0x2; // select 13Mhz clock
	spin(10);

#if 1
/* kick the clock speed up */
	*REG_H(ARM_DPLL1_CTL_REG) = 0x2830;
	while ((*REG_H(ARM_DPLL1_CTL_REG) & 0x1) == 0)
		;
	spin(1);
	*REG_H(ARM_CKCTL) = 0x300E;
	spin(1);
	*REG(0xFFFECC1C) = 0x88011131;
	spin(1);
#endif
	platform_mux_init();

/* bringup the memory controller */
	// write a series of commands to the memory
	*REG(EMIFF_CMD) = 1; // PRECHARGE
	spin(10);
	*REG(EMIFF_CMD) = 2; // AUTOREFRESH
	spin(10);
	*REG(EMIFF_CMD) = 2; // AUTOREFRESH
	spin(10);
	*REG(EMIFF_MRS_NEW) = (3<<4) | 7; // CAS3, full page burst
	spin(10);

	*REG(EMIFF_OP) = (1<<2); // HPHB mode, regular SDR SDRAM
	*REG(EMIFF_DLL_URD_CTRL) = 0; // DLL disabled
	*REG(EMIFF_DLL_WRD_CTRL) = 0; // DLL disabled
	*REG(EMIFF_EMRS0) = 0; // DLL disabled

	*REG(EMIFF_CONFIG) = (2000<<8) | (0xf<<4) | (1<<2) | (1<<1);

/* test memory */
	if(0) {
		unsigned int i;

		dprintf("starting sdram test...\n");

		dprintf("filling sdram with known pattern...\n");
		for(i=SDRAM_BASE; i < (SDRAM_BASE + SDRAM_SIZE); i += 4) {
			*(unsigned int *)i = i;
		}

		dprintf("checking pattern...\n");
		for(i=SDRAM_BASE; i < (SDRAM_BASE + SDRAM_SIZE); i += 4) {
			if(*(unsigned int *)i != i) {
				dprintf("memory doesn't match at 0x%x: 0x%x\n", i, *(unsigned int *)i);
				LED_DISPLAY_NUM(i, false);
			}
		}
		dprintf("sdram passed memory test\n");
	}
}

void LED_DISPLAY_NUM(int n, bool loop)
{
#define NTOH(n) ((((n)&0xf) <= 9) ? (((n)&0xf) + '0') : (((n)&0xf) - 10 + 'A'))

	LED_DISPLAY(3, NTOH(n));
	LED_DISPLAY(2, NTOH(n>>4));
	LED_DISPLAY(1, NTOH(n>>8));
	LED_DISPLAY(0, NTOH(n>>12));
}

void spin(int usecs)
{
	int i;

	for(i=0; i < usecs * 200; i++) {
		asm("nop");
	}
}

void *kmalloc(unsigned int size)
{
//	dprintf("kmalloc: size %d, ptr %p\n", size, heap_ptr - size);

	return (heap_ptr += size);
}

void kfree(void *ptr)
{
}

int dprintf(const char *fmt, ...)
{
	return 0;
}

int panic(const char *fmt, ...)
{
	LED_DISPLAY(3, '9');
	return 0;
}

