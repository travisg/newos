// Huge sections stolen from OpenBLT
// Will be redone later

#include "stage2.h"
#include "console.h"
#include "debug.h"
#include "vm.h"
#include "kernel.h"
#include "printf.h"
#include "string.h"

#include "arch_cpu.h"
#include "arch_vm.h"
#include "arch_smp.h"
#include "arch_smp_priv.h"

static int num_cpus = 0;

static const unsigned int temp_gdt[] = {
								0x00000000, 0x00000000,
								0x0000ffff, 0x00cf9a00,
								0x0000ffff, 0x00cf9300 };

static unsigned int mp_mem_phys = 0;
static unsigned int mp_mem_virt = 0;
static struct mp_flt_struct *mp_flt_ptr = NULL;
static unsigned int *apic_phys = NULL;
static unsigned int *apic = NULL;
static unsigned int cpu_apic_id[SMP_MAX_CPUS] = { 0, 0};
static unsigned int cpu_os_id[SMP_MAX_CPUS] = { 0, 0};
static unsigned int cpu_apic_version[SMP_MAX_CPUS] = { 0, 0};
static unsigned int *ioapic_phys = NULL; 
static unsigned int *ioapic = NULL; 

void smp_trampoline();
void smp_trampoline_end();

unsigned int apic_read(unsigned int *addr)
{
	return *addr;
}

void apic_write(unsigned int *addr, unsigned int data)
{
	*addr = data;
}

static void *mp_virt_to_phys(void *ptr)
{
	return ((void *)(((unsigned int)ptr - mp_mem_virt) + mp_mem_phys));
}

static void *mp_phys_to_virt(void *ptr)
{	
	return ((void *)(((unsigned int)ptr - mp_mem_phys) + mp_mem_virt));
}

static unsigned int *smp_probe(unsigned int base, unsigned int limit)
{
	unsigned int *ptr;

	dprintf("smp_probe: entry base 0x%x, limit 0x%x\n", base, limit);

	for (ptr = (unsigned int *) base; (unsigned int) ptr < limit; ptr++) {
		if (*ptr == MP_FLT_SIGNATURE) {
			dprintf("smp_probe: found floating pointer structure at %x\n", ptr);
			return ptr;
		}
	}
	return NULL;
}	

static void smp_do_config (void)
{
	char *ptr;
	int i;
	struct mp_config_table *mpc;
	struct mp_ext_pe *pe;
	struct mp_ext_ioapic *io;
	const char *cpu_family[] = { "", "", "", "", "Intel 486",
		"Intel Pentium", "Intel Pentium Pro", "Intel Pentium II" };

	/*
	 * we are not running in standard configuration, so we have to look through
	 * all of the mp configuration table crap to figure out how many processors
	 * we have, where our apics are, etc.
	 */
	num_cpus = 0;

	dprintf("mp_flt_ptr->mpc = %p\n", mp_flt_ptr->mpc);
	mpc = mp_phys_to_virt(mp_flt_ptr->mpc);

	/* print out our new found configuration. */
	ptr = (char *) &(mpc->oem[0]);
	dprintf ("smp: oem id: %c%c%c%c%c%c%c%c product id: "
		"%c%c%c%c%c%c%c%c%c%c%c%c\n", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
		ptr[5], ptr[6], ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12],
		ptr[13], ptr[14], ptr[15], ptr[16], ptr[17], ptr[18], ptr[19],
		ptr[20]);
	dprintf("smp: base table has %d entries, extended section %d bytes\n",
		mpc->num_entries, mpc->ext_len);
	apic_phys = (void *)mpc->apic;

	ptr = (char *) ((unsigned int) mpc + sizeof (struct mp_config_table));
	for (i = 0; i < mpc->num_entries; i++) {
		switch (*ptr) {
			case MP_EXT_PE:
				pe = (struct mp_ext_pe *) ptr;
				cpu_apic_id[num_cpus] = pe->apic_id;
				cpu_os_id[pe->apic_id] = num_cpus;
				cpu_apic_version [num_cpus] = pe->apic_version;
				dprintf ("smp: cpu#%d: %s, apic id %d, version %d%s\n",
					num_cpus++, cpu_family[(pe->signature & 0xf00) >> 8],
					pe->apic_id, pe->apic_version, (pe->cpu_flags & 0x2) ?
					", BSP" : "");
				ptr += 20;
				break;
			case MP_EXT_BUS:
				ptr += 8;
				break;
			case MP_EXT_IO_APIC:
				io = (struct mp_ext_ioapic *) ptr;
				ioapic_phys = io->addr;
				dprintf("smp: found io apic with apic id %d, version %d\n",
					io->ioapic_id, io->ioapic_version);
				ptr += 8;
				break;
			case MP_EXT_IO_INT:
				ptr += 8;
				break;
			case MP_EXT_LOCAL_INT:
				ptr += 8;
				break;
		}
	}
	dprintf("smp: apic @ 0x%x, i/o apic @ 0x%x, total %d processors detected\n",
		(unsigned int)apic_phys, (unsigned int)ioapic_phys, num_cpus);
}

#define MP_SPOT1_START 0x9fc00
#define MP_SPOT1_END   0xa0000
#define MP_SPOT1_LEN   (MP_SPOT1_END-MP_SPOT1_START)
#define MP_SPOT2_START 0xf0000
#define MP_SPOT2_END   0x100000
#define MP_SPOT2_LEN   (MP_SPOT2_END-MP_SPOT2_START)

static int arch_smp_find_mp_config()
{
	unsigned int ptr;

/*
	vm_map_physical_memory(vm_get_kernel_aspace(), "smptemp1", &ptr, AREA_ANY_ADDRESS,
		MP_SPOT1_LEN, 0, MP_SPOT1_START);
	dprintf("ptr = 0x%x\n", ptr);
	smp_probe(*ptr, *ptr + MP_SPOT1_LEN);
*/
	vm_map_physical_memory(vm_get_kernel_aspace(), "smp_mp", (void **)&ptr, AREA_ANY_ADDRESS,
		MP_SPOT2_LEN, 0, MP_SPOT2_START);
	dprintf("ptr = 0x%x\n", ptr);
	mp_flt_ptr = (struct mp_flt_struct *)smp_probe(ptr, ptr + MP_SPOT2_LEN);
	if(mp_flt_ptr != NULL) {
		mp_mem_phys = MP_SPOT2_START;
		mp_mem_virt = ptr;

		dprintf ("arch_smp_init: intel mp version %s, %s", (mp_flt_ptr->mp_rev == 1) ? "1.1" :
			"1.4", (mp_flt_ptr->mp_feature_2 & 0x80) ?
			"imcr and pic compatibility mode.\n" : "virtual wire compatibility mode.\n");
		
		if (mp_flt_ptr->mpc == 0) {
			/* this system conforms to one of the default configurations */
//			mp_num_def_config = mp_flt_ptr->mp_feature_1;
			dprintf ("smp: standard configuration %d\n", mp_flt_ptr->mp_feature_1);
/*			num_cpus = 2;
			cpu_apic_id[0] = 0;
			cpu_apic_id[1] = 1;
			apic_phys = (unsigned int *) 0xfee00000;
			ioapic_phys = (unsigned int *) 0xfec00000;
			kprintf ("smp: WARNING: standard configuration code is untested");
*/
		} else {
			dprintf("not default config\n");
			smp_do_config();
		}
		return num_cpus;
	} else {
		num_cpus = 1;
		return 1;
	}
}

static int arch_smp_cpu_ready()
{
	dprintf("arch_smp_cpu_ready: entry cpu %d\n", arch_smp_get_current_cpu());

	for(;;);

	return 0;
}

static unsigned int arch_smp_get_cr3()
{
	unsigned int cr3 = 0;
	
	asm volatile("movl %%cr3,%0" : "=g" (cr3));
	return cr3;
}

static int arch_smp_boot_all_cpus()
{
	unsigned int trampoline_code_phys;
	unsigned int trampoline_code_virt;
	unsigned int trampoline_stack_phys;
	unsigned int trampoline_stack_virt;
	int i;
	
	// allocate a stack and a code area for the smp trampoline
	// (these have to be < 1M physical)
	trampoline_code_phys = 0xa0000 - PAGE_SIZE; // 640kB - 4096
	vm_map_physical_memory(vm_get_kernel_aspace(), "trampoline code",
		(void **)&trampoline_code_virt, AREA_ANY_ADDRESS, PAGE_SIZE, 0,
		trampoline_code_phys);
	dprintf("trampoline code page at vaddr 0x%p\n", trampoline_code_virt);

	trampoline_stack_phys = 0xa0000 - 2 * PAGE_SIZE; // 640kB - 8192
	vm_map_physical_memory(vm_get_kernel_aspace(), "trampoline stack",
		(void **)&trampoline_stack_virt, AREA_ANY_ADDRESS, PAGE_SIZE, 0,
		trampoline_stack_phys);
	dprintf("trampoline stack page at vaddr 0x%p\n", trampoline_stack_virt);

	// hack to map the code & stack identity
	pmap_map_page(trampoline_stack_phys, trampoline_stack_phys);
	pmap_map_page(trampoline_code_phys, trampoline_code_phys);


	// copy the trampoline code over
	memcpy((char *)trampoline_code_virt, &smp_trampoline,
		(unsigned int)&smp_trampoline_end - (unsigned int)&smp_trampoline);
	
	// boot the cpus
	for(i = 1; i < num_cpus; i++) {
		unsigned int *final_stack;
		unsigned int *tramp_stack_ptr;
		unsigned int config;
		int num_startups;
		int j;
		char temp[64];
		
		// create a final stack the trampoline code will put the ap processor on
		sprintf(temp, "idle_thread%d_kstack", i);
		vm_create_area(vm_get_kernel_aspace(), temp, (void **)&final_stack,
			AREA_ANY_ADDRESS, KSTACK_SIZE, 0);
			
		// set this stack up
		memset(final_stack, 0, KSTACK_SIZE);
		*(final_stack + (KSTACK_SIZE / sizeof(unsigned int)) - 1) =
			(unsigned int)&arch_smp_cpu_ready;
		
		// set the trampoline stack up
		tramp_stack_ptr = (unsigned int *)(trampoline_stack_virt + PAGE_SIZE - 4);
		// final location of the stack
		*tramp_stack_ptr = ((unsigned int)final_stack) + KSTACK_SIZE - sizeof(unsigned int);
		tramp_stack_ptr--;
		// page dir
		*tramp_stack_ptr = arch_smp_get_cr3();
		tramp_stack_ptr--;
		
		// put a gdt descriptor at the bottom of the stack
		*((unsigned short *)trampoline_stack_virt) = 0x18-1; // LIMIT
		*((unsigned int *)(trampoline_stack_virt + 2)) = trampoline_stack_phys + 8;
		// put the gdt at the bottom
		memcpy(&((unsigned int *)trampoline_stack_virt)[2], temp_gdt, sizeof(temp_gdt));

		/* clear apic errors */
		if(cpu_apic_version[i] & 0xf0) {
			apic_write(APIC_ESR, 0);
			apic_read(APIC_ESR);
		}

		/* send (aka assert) INIT IPI */
		config = (apic_read(APIC_ICR2) & 0x00ffffff) | (cpu_apic_id[i] << 24);
		apic_write(APIC_ICR2, config); /* set target pe */
//		config = (apic_read(APIC_ICR1) & 0xfff0f800) | APIC_DM_INIT | 
//		config = (apic_read(APIC_ICR1) & 0xfff33000) | APIC_DM_INIT | 
//			APIC_LEVEL_TRIG | APIC_ASSERT;
		config = (apic_read(APIC_ICR1) & 0xfff00000) | 0x0000c500;
		apic_write(APIC_ICR1, config);

		// wait for pending to end
		while((apic_read(APIC_ICR1) & 0x00001000) == 0x00001000);

		/* deassert INIT */
		config = (apic_read(APIC_ICR2) & 0x00ffffff) | (cpu_apic_id[i] << 24);
		apic_write(APIC_ICR2, config);
//		config = (apic_read(APIC_ICR1) & ~0xcdfff) | APIC_LEVEL_TRIG | APIC_DM_INIT;
		config = (apic_read(APIC_ICR1) & 0xfff00000) | 0x00008500;

		// wait for pending to end
		while((apic_read(APIC_ICR1) & 0x00001000) == 0x00001000)
			dprintf("0x%x\n", apic_read(APIC_ICR1));

		/* wait 10ms */
//		u_sleep (10000);
		for(j=0; j<100000000; j++)
			j=j;
		
		/* is this a local apic or an 82489dx ? */
		num_startups = (cpu_apic_version[i] & 0xf0) ? 2 : 0;
		for (j = 0; j < num_startups; j++) {
			int j1;
			
			/* it's a local apic, so send STARTUP IPIs */
			dprintf("smp: sending STARTUP\n");
			apic_write(APIC_ESR, 0);

			/* set target pe */
			config = (apic_read(APIC_ICR2) & 0xf0ffffff) | (cpu_apic_id[i] << 24);
			apic_write(APIC_ICR2, config);

			/* send the IPI */
			config = (apic_read(APIC_ICR1) & 0xfff0f800) | APIC_DM_STARTUP |
				(0x9f000 >> 12);
			apic_write(APIC_ICR1, config);

			/* wait */
			for(j1=0; j1<100000000; j1++)
				j1=j1;
//			u_sleep (200);
			while((apic_read(APIC_ICR1)& 0x00001000) == 0x00001000);
		}
	} 

	pmap_unmap_page(trampoline_stack_phys);
	pmap_unmap_page(trampoline_code_phys);

	return 0;
}

int arch_smp_init(struct kernel_args *ka)
{
	dprintf("arch_smp_init: entry\n");

	if(arch_smp_find_mp_config() > 1) {
		dprintf("arch_smp_init: had found > 1 cpus\n");
		dprintf("post config:\n");
		dprintf("num_cpus = 0x%p\n", num_cpus);
		dprintf("apic_phys = 0x%p\n", apic_phys);
		dprintf("ioapic_phys = 0x%p\n", ioapic_phys);
	
		// map in the apic & ioapic
		vm_map_physical_memory(vm_get_kernel_aspace(), "local_apic", (void **)&apic, AREA_ANY_ADDRESS,
			4096, 0, (unsigned int)apic_phys);
		vm_map_physical_memory(vm_get_kernel_aspace(), "ioapic", (void **)&ioapic, AREA_ANY_ADDRESS,
			4096, 0, (unsigned int)ioapic_phys);
	
		// unmap the smp config stuff
		// vm_free_area(...);
	
		dprintf("apic = 0x%p\n", apic);
		dprintf("ioapic = 0x%p\n", ioapic);
	
		// set up the apic
		dprintf("setting up the apic...");
		{
			unsigned int config;
			config = apic_read(APIC_SIVR) & 0xfffffc00;
			config |= APIC_ENABLE | 0xff; /* set spurious interrupt vector to 0xff */
			dprintf("writing 0x%x\n", config);
			apic_write(APIC_SIVR, config);
			config = apic_read(APIC_TPRI) & 0xffffff00; /* accept all interrupts */
			apic_write(APIC_TPRI, config);
		
			config = apic_read(APIC_SIVR);
			dprintf("reading config back from SIVR = 0x%x\n", apic_read(APIC_SIVR));
			apic_write(APIC_EOI, 0);
		
			config = (apic_read(APIC_LVT3) & 0xffffff00) | 0xfe; /* XXX - set vector */
			apic_write(APIC_LVT3, config);
		}
		dprintf("done\n");
	
		dprintf("trampolining other cpus\n");
		arch_smp_boot_all_cpus();
		dprintf("done trampolining\n");
	}

	dprintf("arch_smp_init: exit\n");

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

