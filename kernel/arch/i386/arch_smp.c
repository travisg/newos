// Huge sections stolen from OpenBLT
// Will be redone later

#include "stage2.h"
#include "console.h"
#include "debug.h"
#include "vm.h"

#include "arch_smp.h"

#define SMP_MAX_CPUS 2
static int num_cpus = 0;

//#define MP_FLT_SIGNATURE   (('_' << 24) | ('P' << 16) | ('M' << 8) | ('_'))
#define MP_FLT_SIGNATURE '_PM_'
#define MP_CTH_SIGNATURE   (('P' << 24) | ('C' << 16) | ('M' << 8) | ('P'))

#define APIC_DM_INIT       (5 << 8)
#define APIC_DM_STARTUP    (6 << 8)
#define APIC_LEVEL_TRIG    (1 << 14)
#define APIC_ASSERT        (1 << 13)

#define APIC_ENABLE        0x100
#define APIC_FOCUS         (~(1 << 9))
#define APIC_SIV           (0xff)

#define APIC_ID            ((unsigned int *) ((unsigned int) apic + 0x020))
#define APIC_VERSION       ((unsigned int *) ((unsigned int) apic + 0x030))
#define APIC_TPRI          ((unsigned int *) ((unsigned int) apic + 0x080))
#define APIC_EOI           ((unsigned int *) ((unsigned int) apic + 0x0b0))
#define APIC_SIVR          ((unsigned int *) ((unsigned int) apic + 0x0f0))
#define APIC_ESR           ((unsigned int *) ((unsigned int) apic + 0x280))
#define APIC_ICR1          ((unsigned int *) ((unsigned int) apic + 0x300))
#define APIC_ICR2          ((unsigned int *) ((unsigned int) apic + 0x310))
#define APIC_LVTT          ((unsigned int *) ((unsigned int) apic + 0x320))
#define APIC_LINT0         ((unsigned int *) ((unsigned int) apic + 0x350))
#define APIC_LVT3          ((unsigned int *) ((unsigned int) apic + 0x370))
#define APIC_ICRT          ((unsigned int *) ((unsigned int) apic + 0x380))
#define APIC_CCRT          ((unsigned int *) ((unsigned int) apic + 0x390))
#define APIC_TDCR          ((unsigned int *) ((unsigned int) apic + 0x3e0))

#define APIC_TDCR_2        0x00
#define APIC_TDCR_4        0x01
#define APIC_TDCR_8        0x02
#define APIC_TDCR_16       0x03
#define APIC_TDCR_32       0x08
#define APIC_TDCR_64       0x09
#define APIC_TDCR_128      0x0a
#define APIC_TDCR_1        0x0b

#define APIC_LVTT_VECTOR   0x000000ff
#define APIC_LVTT_DS       0x00001000
#define APIC_LVTT_M        0x00010000
#define APIC_LVTT_TM       0x00020000

#define APIC_LVT_DM        0x00000700
#define APIC_LVT_IIPP      0x00002000
#define APIC_LVT_TM        0x00008000
#define APIC_LVT_M         0x00010000
#define APIC_LVT_OS        0x00020000

#define APIC_TPR_PRIO      0x000000ff
#define APIC_TPR_INT       0x000000f0
#define APIC_TPR_SUB       0x0000000f

#define APIC_SVR_SWEN      0x00000100
#define APIC_SVR_FOCUS     0x00000200

#define APIC_DEST_STARTUP  0x00600

#define LOPRIO_LEVEL       0x00000010

#define APIC_DEST_FIELD          (0)
#define APIC_DEST_SELF           (1 << 18)
#define APIC_DEST_ALL            (2 << 18)
#define APIC_DEST_ALL_BUT_SELF   (3 << 18)

#define IOAPIC_ID          0x0
#define IOAPIC_VERSION     0x1
#define IOAPIC_ARB         0x2
#define IOAPIC_REDIR_TABLE 0x10

#define IPI_CACHE_FLUSH    0x40
#define IPI_INV_TLB        0x41
#define IPI_INV_PTE        0x42
#define IPI_INV_RESCHED    0x43
#define IPI_STOP           0x44

#define MP_EXT_PE          0
#define MP_EXT_BUS         1
#define MP_EXT_IO_APIC     2
#define MP_EXT_IO_INT      3
#define MP_EXT_LOCAL_INT   4

#define MP_EXT_PE_LEN          20
#define MP_EXT_BUS_LEN         8
#define MP_EXT_IO_APIC_LEN     8
#define MP_EXT_IO_INT_LEN      8
#define MP_EXT_LOCAL_INT_LEN   8

struct mp_config_table {
	unsigned int signature;       /* "PCMP" */
	unsigned short table_len;     /* length of this structure */
	unsigned char mp_rev;         /* spec supported, 1 for 1.1 or 4 for 1.4 */
	unsigned char checksum;       /* checksum, all bytes add up to zero */
	char oem[8];                  /* oem identification, not null-terminated */
	char product[12];             /* product name, not null-terminated */
	void *oem_table_ptr;          /* addr of oem-defined table, zero if none */
	unsigned short oem_len;       /* length of oem table */
	unsigned short num_entries;   /* number of entries in base table */
	unsigned int apic;            /* address of apic */
	unsigned short ext_len;       /* length of extended section */
	unsigned char ext_checksum;   /* checksum of extended table entries */
};

struct mp_flt_struct {
	unsigned int signature;       /* "_MP_" */
	struct mp_config_table *mpc;  /* address of mp configuration table */
	unsigned char mpc_len;        /* length of this structure in 16-byte units */
	unsigned char mp_rev;         /* spec supported, 1 for 1.1 or 4 for 1.4 */
	unsigned char checksum;       /* checksum, all bytes add up to zero */
	unsigned char mp_feature_1;   /* mp system configuration type if no mpc */
	unsigned char mp_feature_2;   /* imcrp */
	unsigned char mp_feature_3, mp_feature_4, mp_feature_5; /* reserved */
};

struct mp_ext_pe
{
	unsigned char type;
	unsigned char apic_id;
	unsigned char apic_version;
	unsigned char cpu_flags;
	unsigned int signature;       /* stepping, model, family, each four bits */
	unsigned int feature_flags;
	unsigned int res1, res2;
};

struct mp_ext_ioapic
{
	unsigned char type;
	unsigned char ioapic_id;
	unsigned char ioapic_version;
	unsigned char ioapic_flags;
	unsigned int *addr;
};

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
	apic_phys = (unsigned int *)mp_phys_to_virt(mpc->apic);

	ptr = (char *) ((unsigned int) mpc + sizeof (struct mp_config_table));
	for (i = 0; i < mpc->num_entries; i++)
		switch (*ptr)
		{
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

	dprintf("smp: apic @ 0x%x, i/o apic @ 0x%x, total %d processors detected\n",
		(unsigned int)apic_phys, (unsigned int)ioapic_phys, num_cpus);
}

int arch_smp_init(struct kernel_args *ka)
{
	unsigned int ptr;
	dprintf("arch_smp_init: entry\n");

#define MP_SPOT1_START 0x9fc00
#define MP_SPOT1_END   0xa0000
#define MP_SPOT1_LEN   (MP_SPOT1_END-MP_SPOT1_START)
#define MP_SPOT2_START 0xf0000
#define MP_SPOT2_END   0x100000
#define MP_SPOT2_LEN   (MP_SPOT2_END-MP_SPOT2_START)

/*
	vm_map_physical_memory(vm_get_kernel_aspace(), "smptemp1", &ptr, AREA_ANY_ADDRESS,
		MP_SPOT1_LEN, 0, MP_SPOT1_START);
	dprintf("ptr = 0x%x\n", ptr);
	smp_probe(*ptr, *ptr + MP_SPOT1_LEN);
*/
	vm_map_physical_memory(vm_get_kernel_aspace(), "smp_mp", &ptr, AREA_ANY_ADDRESS,
		MP_SPOT2_LEN, 0, MP_SPOT2_START);
	dprintf("ptr = 0x%x\n", ptr);
	mp_flt_ptr = smp_probe(ptr, ptr + MP_SPOT2_LEN);
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
	} else {
		num_cpus = 0;
	}

	dprintf("post config:\n");
	dprintf("num_cpus = 0x%p\n", num_cpus);
	dprintf("apic_phys = 0x%p\n", apic_phys);
	dprintf("ioapic_phys = 0x%p\n", ioapic_phys);

	// map in the apic & ioapic
	vm_map_physical_memory(vm_get_kernel_aspace(), "local_apic", &apic, AREA_ANY_ADDRESS,
		4096, 0, (unsigned int)apic_phys);
	vm_map_physical_memory(vm_get_kernel_aspace(), "ioapic", &ioapic, AREA_ANY_ADDRESS,
		4096, 0, (unsigned int)ioapic_phys);

	// unmap the smp config stuff
	// vm_free_area(...);

	dprintf("apic = 0x%p\n", apic);
	dprintf("ioapic = 0x%p\n", ioapic);

	// set up the apic
	dprintf("setting up the apic...");
	{
		unsigned int config;
		config = (apic_read((unsigned int *) APIC_SIVR) & APIC_FOCUS) | APIC_ENABLE |
			0xff; /* set spurious interrupt vector to 0xff */
		apic_write(APIC_SIVR, config);
		config = apic_read(APIC_TPRI) & 0xffffff00; /* accept all interrupts */
		apic_write(APIC_TPRI, config);
	
		apic_read(APIC_SIVR);
		apic_write(APIC_EOI, 0);
	
		config = (apic_read(APIC_LVT3) & 0xffffff00) | 0xfe; /* XXX - set vector */
		apic_write(APIC_LVT3, config);
	}
	dprintf("done\n");

	dprintf("arch_smp_init: exit\n");

	return 0;
}

int arch_smp_get_num_cpus()
{
	return num_cpus;
}

int arch_smp_get_current_cpu()
{
	return 0;
}