/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/cpu.h>
#include <kernel/arch/cpu.h>
#include <kernel/heap.h>
#include <kernel/vm.h>
#include <kernel/debug.h>
#include <kernel/smp.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/arch/i386/selector.h>
#include <kernel/arch/int.h>
#include <kernel/arch/i386/interrupts.h>
#include <newos/errors.h>

#include <boot/stage2.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* a few debug functions that get added to the kernel debugger menu */
static void dbg_in(int argc, char **argv);
static void dbg_out(int argc, char **argv);

static struct tss **tss;
static int *tss_loaded;

/* tss to switch to a special 'task' on the double fault handler */
static struct tss double_fault_tss;
static uint32 double_fault_stack[1024];

static desc_table *gdt = 0;

/* cpu vendor info */
struct cpu_vendor_info {
	const char *vendor;
	const char *ident_string[2];
};

static const struct cpu_vendor_info vendor_info[VENDOR_NUM] = {
	{ "Intel", { "GenuineIntel" } },
	{ "AMD", { "AuthenticAMD" } },
	{ "Cyrix", { "CyrixInstead" } },
	{ "UMC", { "UMC UMC UMC" } },
	{ "NexGen", { "NexGenDriven" } },
	{ "Centaur", { "CentaurHauls" } },
	{ "Rise", { "RiseRiseRise" } },
	{ "Transmeta", { "GenuineTMx86", "TransmetaCPU" } },
	{ "NSC", { "Geode by NSC" } },
};

int arch_cpu_preboot_init(kernel_args *ka)
{
	write_dr3(0);
	return 0;
}

static void make_feature_string(cpu_ent *cpu, char *str)
{

	str[0] = 0;

	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_FPU)
		strcat(str, "fpu ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_VME)
		strcat(str, "vme ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_DE)
		strcat(str, "de ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_PSE)
		strcat(str, "pse ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_TSC)
		strcat(str, "tsc ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_MSR)
		strcat(str, "msr ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_PAE)
		strcat(str, "pae ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_MCE)
		strcat(str, "mce ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_CX8)
		strcat(str, "cx8 ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_APIC)
		strcat(str, "apic ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_SEP)
		strcat(str, "sep ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_MTRR)
		strcat(str, "mtrr ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_PGE)
		strcat(str, "pge ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_MCA)
		strcat(str, "mca ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_CMOV)
		strcat(str, "cmov ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_PAT)
		strcat(str, "pat ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_PSE36)
		strcat(str, "pse36 ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_PSN)
		strcat(str, "psn ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_CLFSH)
		strcat(str, "clfsh ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_DS)
		strcat(str, "ds ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_ACPI)
		strcat(str, "acpi ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_MMX)
		strcat(str, "mmx ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_FXSR)
		strcat(str, "fxsr ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_SSE)
		strcat(str, "sse ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_SSE2)
		strcat(str, "sse2 ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_SS)
		strcat(str, "ss ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_HTT)
		strcat(str, "htt ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_TM)
		strcat(str, "tm ");
	if(cpu->info.arch.feature[FEATURE_COMMON] & X86_PBE)
		strcat(str, "pbe ");
	if(cpu->info.arch.feature[FEATURE_EXT] & X86_EXT_SSE3)
		strcat(str, "sse3 ");
	if(cpu->info.arch.feature[FEATURE_EXT] & X86_EXT_MONITOR)
		strcat(str, "monitor ");
	if(cpu->info.arch.feature[FEATURE_EXT] & X86_EXT_DSCPL)
		strcat(str, "dscpl ");
	if(cpu->info.arch.feature[FEATURE_EXT] & X86_EXT_EST)
		strcat(str, "est ");
	if(cpu->info.arch.feature[FEATURE_EXT] & X86_EXT_TM2)
		strcat(str, "tm2 ");
	if(cpu->info.arch.feature[FEATURE_EXT] & X86_EXT_CNXTID)
		strcat(str, "cnxtid ");
	if(cpu->info.arch.feature[FEATURE_EXT_AMD] & X86_AMD_EXT_SYSCALL)
		strcat(str, "syscall ");
	if(cpu->info.arch.feature[FEATURE_EXT_AMD] & X86_AMD_EXT_NX)
		strcat(str, "nx ");
	if(cpu->info.arch.feature[FEATURE_EXT_AMD] & X86_AMD_EXT_MMXEXT)
		strcat(str, "mmxext ");
	if(cpu->info.arch.feature[FEATURE_EXT_AMD] & X86_AMD_EXT_FFXSR)
		strcat(str, "ffxsr ");
	if(cpu->info.arch.feature[FEATURE_EXT_AMD] & X86_AMD_EXT_LONG)
		strcat(str, "long ");
	if(cpu->info.arch.feature[FEATURE_EXT_AMD] & X86_AMD_EXT_3DNOWEXT)
		strcat(str, "3dnowext ");
	if(cpu->info.arch.feature[FEATURE_EXT_AMD] & X86_AMD_EXT_3DNOW)
		strcat(str, "3dnow ");
}

static int detect_cpu(kernel_args *ka, int curr_cpu) 
{
	unsigned int data[4];
	char vendor_str[17];
	int i;
	cpu_ent *cpu = get_cpu_struct(curr_cpu);

	// clear out the cpu info data
	cpu->info.arch.vendor = VENDOR_UNKNOWN;
	cpu->info.arch.vendor_name = "UNKNOWN VENDOR";
	cpu->info.arch.feature[FEATURE_COMMON] = 0;
	cpu->info.arch.feature[FEATURE_EXT] = 0;
	cpu->info.arch.feature[FEATURE_EXT_AMD] = 0;
	cpu->info.arch.model_name[0] = 0;

	// print some fun data
	i386_cpuid(0, data);

	// build the vendor string
	memset(vendor_str, 0, sizeof(vendor_str));
	*(unsigned int *)&vendor_str[0] = data[1];
	*(unsigned int *)&vendor_str[4] = data[3];
	*(unsigned int *)&vendor_str[8] = data[2];

	// get the family, model, stepping
	i386_cpuid(1, data);
	cpu->info.arch.family = (data[0] >> 8) & 0xf;
	cpu->info.arch.model = (data[0] >> 4) & 0xf;
	cpu->info.arch.stepping = data[0] & 0xf;
	dprintf("CPU %d: family %d model %d stepping %d, string '%s'\n",
		curr_cpu, cpu->info.arch.family, cpu->info.arch.model, cpu->info.arch.stepping, vendor_str);
	kprintf("CPU %d: family %d model %d stepping %d, string '%s'\n",
		curr_cpu, cpu->info.arch.family, cpu->info.arch.model, cpu->info.arch.stepping, vendor_str);

	// figure out what vendor we have here

	for(i=0; i<VENDOR_NUM; i++) {
		if(!strcmp(vendor_str, vendor_info[i].ident_string[0])) {
			cpu->info.arch.vendor = i;
			cpu->info.arch.vendor_name = vendor_info[i].vendor;
			break;
		}
		if(!strcmp(vendor_str, vendor_info[i].ident_string[1])) {
			cpu->info.arch.vendor = i;
			cpu->info.arch.vendor_name = vendor_info[i].vendor;
			break;
		}
	}

	// see if we can get the model name
	i386_cpuid(0x80000000, data);
	if(data[0] >= 0x80000004) {
		// build the model string
		memset(cpu->info.arch.model_name, 0, sizeof(cpu->info.arch.model_name));
		i386_cpuid(0x80000002, data);
		memcpy(cpu->info.arch.model_name, data, sizeof(data));
		i386_cpuid(0x80000003, data);
		memcpy(cpu->info.arch.model_name+16, data, sizeof(data));
		i386_cpuid(0x80000004, data);
		memcpy(cpu->info.arch.model_name+32, data, sizeof(data));

		// some cpus return a right-justified string
		for(i = 0; cpu->info.arch.model_name[i] == ' '; i++)
			;
		if(i > 0) {
			memmove(cpu->info.arch.model_name, 
				&cpu->info.arch.model_name[i], 
				strlen(&cpu->info.arch.model_name[i]) + 1);
		}

		dprintf("CPU %d: vendor '%s' model name '%s'\n", 
			curr_cpu, cpu->info.arch.vendor_name, cpu->info.arch.model_name);
		kprintf("CPU %d: vendor '%s' model name '%s'\n",
			curr_cpu, cpu->info.arch.vendor_name, cpu->info.arch.model_name);
	} else {
		strcpy(cpu->info.arch.model_name, "unknown");
	}

	// load feature bits
	i386_cpuid(1, data);
	cpu->info.arch.feature[FEATURE_COMMON] = data[3]; // edx
	cpu->info.arch.feature[FEATURE_EXT] = data[2]; // ecx
	if(cpu->info.arch.vendor == VENDOR_AMD) {
		i386_cpuid(0x80000001, data);
		cpu->info.arch.feature[FEATURE_EXT_AMD] = data[3]; // edx
	}

	make_feature_string(cpu, cpu->info.arch.feature_string);
	dprintf("CPU %d: features: %s\n", curr_cpu, cpu->info.arch.feature_string);
	kprintf("CPU %d: features: %s\n", curr_cpu, cpu->info.arch.feature_string);

	return 0;
}

int arch_cpu_init(kernel_args *ka)
{
	setup_system_time(ka->arch_args.system_time_cv_factor);

	return 0;
}

int arch_cpu_init_percpu(kernel_args *ka, int curr_cpu)
{
	detect_cpu(ka, curr_cpu);

	return 0;
}

int arch_cpu_init2(kernel_args *ka)
{
	region_id rid;
	struct tss_descriptor *tss_d;
	unsigned int i;

	// account for the segment descriptors
	gdt = (desc_table *)ka->arch_args.vir_gdt;
	vm_create_anonymous_region(vm_get_kernel_aspace_id(), "gdt", (void **)&gdt,
		REGION_ADDR_EXACT_ADDRESS, PAGE_SIZE, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

	i386_selector_init( gdt );  // pass the new gdt

	tss = kmalloc(sizeof(struct tss *) * ka->num_cpus);
	if(tss == NULL) {
		panic("arch_cpu_init2: could not allocate buffer for tss pointers\n");
		return ERR_NO_MEMORY;
	}

	tss_loaded = kmalloc(sizeof(int) * ka->num_cpus);
	if(tss == NULL) {
		panic("arch_cpu_init2: could not allocate buffer for tss booleans\n");
		return ERR_NO_MEMORY;
	}
	memset(tss_loaded, 0, sizeof(int) * ka->num_cpus);

	for(i=0; i<ka->num_cpus; i++) {
		char tss_name[16];

		sprintf(tss_name, "tss%d", i);
		rid = vm_create_anonymous_region(vm_get_kernel_aspace_id(), tss_name, (void **)&tss[i],
			REGION_ADDR_ANY_ADDRESS, PAGE_SIZE, REGION_WIRING_WIRED, LOCK_RW|LOCK_KERNEL);
		if(rid < 0) {
			panic("arch_cpu_init2: unable to create region for tss\n");
			return ERR_NO_MEMORY;
		}

		memset(tss[i], 0, sizeof(struct tss));
		tss[i]->ss0 = KERNEL_DATA_SEG;

		// add TSS descriptor for this new TSS
		tss_d = (struct tss_descriptor *)&gdt[6 + i];
		tss_d->limit_00_15 = sizeof(struct tss) & 0xffff;
		tss_d->limit_19_16 = 0; // not this long
		tss_d->base_00_15 = (addr_t)tss[i] & 0xffff;
		tss_d->base_23_16 = ((addr_t)tss[i] >> 16) & 0xff;
		tss_d->base_31_24 = (addr_t)tss[i] >> 24;
		tss_d->type = 0x9;
		tss_d->zero = 0;
		tss_d->dpl = 0;
		tss_d->present = 1;
		tss_d->avail = 0;
		tss_d->zero1 = 0;
		tss_d->zero2 = 1;
		tss_d->granularity = 1;
	}


	/* set up the double fault tss */
	memset(&double_fault_tss, 0, sizeof(double_fault_tss));
	double_fault_tss.sp0 = (uint32)double_fault_stack + sizeof(double_fault_stack);
	double_fault_tss.ss0 = KERNEL_DATA_SEG;
	read_cr3(double_fault_tss.cr3); // copy the current cr3 to the double fault cr3
	double_fault_tss.eip = (uint32)&trap8;
	double_fault_tss.es = KERNEL_DATA_SEG;
	double_fault_tss.cs = KERNEL_CODE_SEG;
	double_fault_tss.ss = KERNEL_DATA_SEG;
	double_fault_tss.ds = KERNEL_DATA_SEG;
	double_fault_tss.fs = KERNEL_DATA_SEG;
	double_fault_tss.gs = KERNEL_DATA_SEG;
	double_fault_tss.ldt_seg_selector = KERNEL_DATA_SEG;

	tss_d = (struct tss_descriptor *)&gdt[5];
	tss_d->limit_00_15 = sizeof(struct tss) & 0xffff;
	tss_d->limit_19_16 = 0; // not this long
	tss_d->base_00_15 = (addr_t)&double_fault_tss & 0xffff;
	tss_d->base_23_16 = ((addr_t)&double_fault_tss >> 16) & 0xff;
	tss_d->base_31_24 = (addr_t)&double_fault_tss >> 24;
	tss_d->type = 0x9; // tss descriptor, not busy
	tss_d->zero = 0;
	tss_d->dpl = 0;
	tss_d->present = 1;
	tss_d->avail = 0;
	tss_d->zero1 = 0;
	tss_d->zero2 = 1;
	tss_d->granularity = 1;

	i386_set_task_gate(8, DOUBLE_FAULT_TSS);

	// set up a few debug commands (in, out)
	dbg_add_command(&dbg_in, "in", "read I/O port");
	dbg_add_command(&dbg_out, "out", "write I/O port");

	return 0;
}

bool i386_check_feature(uint32 feature, enum i386_feature_type type) 
{
	cpu_ent *cpu = get_curr_cpu_struct();
	return cpu->info.arch.feature[type] & feature;
}

desc_table *i386_get_gdt(void)
{
	return gdt;
}

void i386_set_kstack(addr_t kstack)
{
	int curr_cpu = smp_get_current_cpu();

//	dprintf("i386_set_kstack: kstack 0x%x, cpu %d\n", kstack, curr_cpu);
	if(tss_loaded[curr_cpu] == 0) {
		short seg = (TSS + 8*curr_cpu);
		asm("movw  %0, %%ax;"
			"ltr %%ax;" : : "r" (seg) : "eax");
		tss_loaded[curr_cpu] = 1;
	}

	tss[curr_cpu]->sp0 = kstack;
//	dprintf("done\n");
}

void arch_cpu_invalidate_TLB_range(addr_t start, addr_t end)
{
	for(; start < end; start += PAGE_SIZE) {
		invalidate_TLB(start);
	}
}

void arch_cpu_invalidate_TLB_list(addr_t pages[], int num_pages)
{
	int i;
	for(i=0; i<num_pages; i++) {
		invalidate_TLB(pages[i]);
	}
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
	switch(smp_get_num_cpus()) {
		case 0:
			panic("You need at least 1 CPU to run NewOS\n");
		case 1:
			asm("hlt");
		default:
			break;
	}
}

void arch_cpu_sync_icache(void *address, size_t len)
{
	// instruction cache is always consistent on x86
}

static void dbg_in(int argc, char **argv)
{
	int value;
	int port;

	if(argc < 2) {
		dprintf("not enough args\nusage: %s (1|2|4) port\n", argv[0]);
		return;
	}

	port = atoul(argv[2]);

	switch(argv[1][0]) {
		case '1':
		case 'b':
			value = in8(port);
			break;
		case '2':
		case 'h':
			value = in16(port);
			break;
		case '4':
		case 'w':
			value = in32(port);
			break;
		default:
			dprintf("invalid width argument\n");
			return;
	}
	dprintf("I/O port 0x%x = 0x%x\n", port, value);
}

static void dbg_out(int argc, char **argv)
{
	int value;
	int port;

	if(argc < 3) {
		dprintf("not enough args\nusage: %s (1|2|4) port value\n", argv[0]);
		return;
	}

	port = atoul(argv[2]);
	value = atoul(argv[3]);

	switch(argv[1][0]) {
		case '1':
		case 'b':
			out8(value, port);
			break;
		case '2':
		case 'h':
			out16(value, port);
			break;
		case '4':
		case 'w':
			out32(value, port);
			break;
		default:
			dprintf("invalid width argument\n");
			return;
	}
	dprintf("writing 0x%x to I/O port 0x%x\n", value, port);
}

