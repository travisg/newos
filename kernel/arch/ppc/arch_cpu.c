/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/arch/cpu.h>
#include <boot/stage2.h>


int arch_cpu_preboot_init(kernel_args *ka)
{
	return 0;
}

int arch_cpu_init(kernel_args *ka)
{
	return 0;
}

int arch_cpu_init2(kernel_args *ka)
{
	return 0;
}

#define CACHELINE 32

void arch_cpu_sync_icache(void *address, size_t len)
{
	int l, off;
	char *p;

	off = (unsigned int)address & (CACHELINE - 1);
	len += off;

	l = len;
	p = (char *)address - off;
	do {
		asm volatile ("dcbst 0,%0" :: "r"(p));
		p += CACHELINE;
	} while((l -= CACHELINE) > 0);
	asm volatile ("sync");
	p = (char *)address - off;
	do {
		asm volatile ("icbi 0,%0" :: "r"(p));
		p += CACHELINE;
	} while((len -= CACHELINE) > 0);
	asm volatile ("sync");
	asm volatile ("isync");
}

void arch_cpu_invalidate_TLB_range(addr_t start, addr_t end)
{
	asm volatile("sync");
	while(start < end) {
		asm volatile("tlbie %0" :: "r" (start));
		asm volatile("eieio");
		asm volatile("sync");
		start += PAGE_SIZE;
	}
	asm volatile("tlbsync");
	asm volatile("sync");
}

void arch_cpu_invalidate_TLB_list(addr_t pages[], int num_pages)
{
	int i;

	asm volatile("sync");
	for(i = 0; i < num_pages; i++) {
		asm volatile("tlbie %0" :: "r" (pages[i]));
		asm volatile("eieio");
		asm volatile("sync");
	}
	asm volatile("tlbsync");
	asm volatile("sync");
}

void arch_cpu_global_TLB_invalidate(void)
{
	unsigned long i;

	asm volatile("sync");
	for(i=0; i< 0x40000; i += 0x1000) {
		asm volatile("tlbie %0" :: "r" (i));
		asm volatile("eieio");
		asm volatile("sync");
	}
	asm volatile("tlbsync");
	asm volatile("sync");
}

long long system_time(void)
{
	bigtime_t time_base = get_time_base();

	return (time_base * 1000000) / ((66*1000*1000) / 4); // XXX remove hard coded
}

int arch_cpu_user_memcpy(void *to, const void *from, size_t size, addr_t *fault_handler)
{
	char *tmp = (char *)to;
	char *s = (char *)from;

	*fault_handler = (addr_t)&&error;

	while(size--)
		*tmp++ = *s++;

	*fault_handler = 0;

	return 0;
error:
	*fault_handler = 0;
	return ERR_VM_BAD_USER_MEMORY;
}

int arch_cpu_user_strcpy(char *to, const char *from, addr_t *fault_handler)
{
	*fault_handler = (addr_t)&&error;

	while((*to++ = *from++) != '\0')
		;

	*fault_handler = 0;

	return 0;
error:
	*fault_handler = 0;
	return ERR_VM_BAD_USER_MEMORY;
}

int arch_cpu_user_strncpy(char *to, const char *from, size_t size, addr_t *fault_handler)
{
	*fault_handler = (addr_t)&&error;

	while(size-- && (*to++ = *from++) != '\0')
		;

	*fault_handler = 0;

	return 0;
error:
	*fault_handler = 0;
	return ERR_VM_BAD_USER_MEMORY;
}

int arch_cpu_user_memset(void *s, char c, size_t count, addr_t *fault_handler)
{
	char *xs = (char *) s;

	*fault_handler = (addr_t)&&error;

	while (count--)
		*xs++ = c;

	*fault_handler = 0;

	return 0;
error:
	*fault_handler = 0;
	return ERR_VM_BAD_USER_MEMORY;
}

void arch_cpu_idle(void)
{
}
