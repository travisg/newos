/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <string.h>
#include "stage2_priv.h"

void system_reset_exception_entry();
extern int system_reset_exception_entry_end;
void machine_check_exception_entry();
extern int machine_check_exception_entry_end;
void dsi_exception_entry();
extern int dsi_exception_entry_end;
void isi_exception_entry();
extern int isi_exception_entry_end;
void external_interrupt_entry();
extern int external_interrupt_entry_end;
void alignment_exception_entry();
extern int alignment_exception_entry_end;
void program_exception_entry();
extern int program_exception_entry_end;
void decrementer_exception_entry();
extern int decrementer_exception_entry_end;
void system_call_exception_entry();
extern int system_call_exception_entry_end;
void trace_exception_entry();
extern int trace_exception_entry_end;
void floating_point_assist_exception_entry();
extern int floating_point_assist_exception_entry_end;
void g3_exception_entry();
extern int g3_exception_entry_end;

void s2_faults_init(kernel_args *ka)
{
	printf("s2_faults_init: entry\n");
	printf("msr = 0x%x\n", getmsr());
	setmsr(getmsr() & ~0x00000040); // make it look for exceptions in the zero page
	printf("msr = 0x%x\n", getmsr());
/*
	printf("msr = 0x%x\n", getmsr());
	setmsr(getmsr() & ~0x00000030); // no more mmu
	printf("msr = 0x%x\n", getmsr());
*/
	printf("s2_faults_init: foo\n");
	printf("0x%x\n", *(unsigned int *)0x00000100);
	printf("0x%x\n", *(unsigned int *)0x00000104);
	printf("0x%x\n", *(unsigned int *)0x00000108);
	printf("0x%x\n", *(unsigned int *)0x0000010c);
	printf("%d\n", (int)&system_reset_exception_entry_end - (int)&system_reset_exception_entry);
	memcpy((void *)0x00000100, &system_reset_exception_entry,
		(int)&system_reset_exception_entry_end - (int)&system_reset_exception_entry);
	printf("%d\n",
		memcmp((void *)0x00000100, &system_reset_exception_entry,
		(int)&system_reset_exception_entry_end - (int)&system_reset_exception_entry));
	printf("0x%x\n", *(unsigned int *)0x00000100);
	printf("0x%x\n", *(unsigned int *)0x00000104);
	printf("0x%x\n", *(unsigned int *)0x00000108);
	printf("0x%x\n", *(unsigned int *)0x0000010c);
	memcpy((void *)0x00000200, &machine_check_exception_entry,
		(int)&machine_check_exception_entry_end - (int)&machine_check_exception_entry);
	memcpy((void *)0x00000300, &dsi_exception_entry,
		(int)&dsi_exception_entry_end - (int)&dsi_exception_entry);
	memcpy((void *)0x00000400, &isi_exception_entry,
		(int)&isi_exception_entry_end - (int)&isi_exception_entry);
	memcpy((void *)0x00000500, &external_interrupt_entry,
		(int)&external_interrupt_entry_end - (int)&external_interrupt_entry);
	memcpy((void *)0x00000600, &alignment_exception_entry,
		(int)&alignment_exception_entry_end - (int)&alignment_exception_entry);
	memcpy((void *)0x00000700, &program_exception_entry,
		(int)&program_exception_entry_end - (int)&program_exception_entry);
	memcpy((void *)0x00000900, &decrementer_exception_entry,
		(int)&decrementer_exception_entry_end - (int)&decrementer_exception_entry);
	memcpy((void *)0x00000c00, &system_call_exception_entry,
		(int)&system_call_exception_entry_end - (int)&system_call_exception_entry);
	memcpy((void *)0x00000d00, &trace_exception_entry,
		(int)&trace_exception_entry_end - (int)&trace_exception_entry);
	memcpy((void *)0x00000e00, &floating_point_assist_exception_entry,
		(int)&floating_point_assist_exception_entry_end - (int)&floating_point_assist_exception_entry);

	memcpy((void *)0x00000f00, &g3_exception_entry,
		(int)&g3_exception_entry_end - (int)&g3_exception_entry);
	memcpy((void *)0x00001300, &g3_exception_entry,
		(int)&g3_exception_entry_end - (int)&g3_exception_entry);
	memcpy((void *)0x00001400, &g3_exception_entry,
		(int)&g3_exception_entry_end - (int)&g3_exception_entry);
	memcpy((void *)0x00001700, &g3_exception_entry,
		(int)&g3_exception_entry_end - (int)&g3_exception_entry);

	printf("s2_faults_init: exit\n");

	syncicache(0, 0x10000);

#if 1
	// test it
//	printf("test = %d\n", *(int *)0x3);

	// syscall
	asm("sc");

	// turn single-step mode
//	setmsr(getmsr() | 0x00000400);
#endif
}

void system_reset_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("system_reset_exception\n");
	for(;;);
}

void machine_check_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("machine_check_exception\n");
	for(;;);
}

void dsi_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	*(int *)0x16008190 = 0xffffff;
	printf("dsi_exception\n");
	for(;;);
}

void isi_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("isi_exception\n");
	for(;;);
}

void external_interrupt()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("external_interrupt\n");
	for(;;);
}

void alignment_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("alignment_exception\n");
	for(;;);
}

void program_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("program_exception\n");
	for(;;);
}

void decrementer_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("decrementer_exception\n");
	for(;;);
}

void system_call_exception()
{
/*
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
//	printf("system_call_exception\n");
//	for(;;);
*/
}

void trace_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("trace_exception\n");
	for(;;);
}

void floating_point_assist_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("floating_point_assist_exception\n");
	for(;;);
}

void g3_exception_entry_exception()
{
	{
		int i;
		for(i = 0; i < 128*1024; i++) {
			((char *)0x96008000)[i] = i;
		}
	}
	printf("g3_exception_entry_exception\n");
	for(;;);
}

