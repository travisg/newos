// Huge sections stolen from OpenBLT
// Will be redone later

#include "stage2.h"
#include "console.h"
#include "debug.h"
#include "vm.h"
#include "kernel.h"
#include "printf.h"
#include "string.h"
#include "int.h"

#include "arch_cpu.h"
#include "arch_vm.h"
#include "arch_smp.h"
#include "arch_smp_priv.h"

static int num_cpus = 0;
static unsigned int *apic = NULL;
static unsigned int cpu_apic_id[SMP_MAX_CPUS] = { 0, 0};
static unsigned int cpu_os_id[SMP_MAX_CPUS] = { 0, 0};
static unsigned int cpu_apic_version[SMP_MAX_CPUS] = { 0, 0};
static unsigned int *ioapic = NULL; 

int boot_cpu_spin[SMP_MAX_CPUS] = { 0, };

int arch_smp_trap_non_boot_cpus(struct kernel_args *ka, int cpu)
{
	if(cpu > 0)
		boot_cpu_spin[cpu] = 1;

	while(boot_cpu_spin[cpu] == 1);

	return cpu;
}

void arch_smp_wake_up_cpu(int cpu)
{
	boot_cpu_spin[cpu] = 0;
}

static unsigned int apic_read(unsigned int *addr)
{
	return *addr;
}

static void apic_write(unsigned int *addr, unsigned int data)
{
	*addr = data;
}

int arch_smp_init(struct kernel_args *ka)
{
	dprintf("arch_smp_init: entry\n");

	if(ka->num_cpus > 1) {
		// XXX remove
		if(boot_cpu_spin[1] == 1)
			dprintf("cpu 1 is spinning\n");	
		
		
		// setup some globals
		num_cpus = ka->num_cpus;
		apic = ka->apic;
		ioapic = ka->ioapic;
		memcpy(cpu_apic_id, ka->cpu_apic_id, sizeof(ka->cpu_apic_id));
		memcpy(cpu_os_id, ka->cpu_os_id, sizeof(ka->cpu_os_id));
		memcpy(cpu_apic_version, ka->cpu_apic_version, sizeof(ka->cpu_apic_version));
	
		// setup areas that represent the apic & ioapic
		vm_create_area(vm_get_kernel_aspace(), "local_apic", (void *)&apic, AREA_ALREADY_MAPPED, PAGE_SIZE, 0);
		vm_create_area(vm_get_kernel_aspace(), "ioapic", (void *)&ioapic, AREA_ALREADY_MAPPED, PAGE_SIZE, 0);
	} else {
		num_cpus = 1;
	}
	return 0;
}

int arch_smp_get_num_cpus()
{
	return num_cpus;
}

int arch_smp_get_current_cpu()
{
	if(apic == NULL)
		return 0;
	else
		return cpu_os_id[(apic_read(APIC_ID) & 0xffffffff) >> 24];
}

int i386_smp_interrupt(int vector)
{
	int retval;
	
	switch(vector) {
		case 0x254:
			// gin-u-wine inter-cpu interrupt
			retval = INT_RESCHEDULE;
			break;
		case 0x255:
			// spurious interrupt
			retval = INT_NO_RESCHEDULE;
			break;
		default:
			// huh?
			retval = INT_NO_RESCHEDULE;
	}

	return retval;
}
