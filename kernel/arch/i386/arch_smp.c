/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <boot/stage2.h>
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/vm.h>
#include <kernel/kernel.h>
#include <kernel/int.h>
#include <kernel/smp.h>
#include <kernel/smp_priv.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/vm.h>
#include <kernel/arch/smp.h>

#include <kernel/arch/i386/smp_priv.h>
#include <kernel/arch/i386/timer.h>

#include <string.h>
#include <stdio.h>

static int num_cpus = 1;
static unsigned int *apic = NULL;
static unsigned int cpu_apic_id[_MAX_CPUS] = { 0 };
static unsigned int cpu_os_id[_MAX_CPUS] = { 0 };
static unsigned int cpu_apic_version[_MAX_CPUS] = { 0 };
static unsigned int *ioapic = NULL;
static unsigned int apic_timer_tics_per_sec = 0;

static int i386_timer_interrupt(void* data)
{
    arch_smp_ack_interrupt();

    return apic_timer_interrupt();
}

static int i386_ici_interrupt(void* data)
{
    // gin-u-wine inter-cpu interrupt
//  dprintf("inter-cpu interrupt on cpu %d\n", arch_smp_get_current_cpu());
    arch_smp_ack_interrupt();

    return smp_intercpu_int_handler();
}

static int i386_spurious_interrupt(void* data)
{
    // spurious interrupt
//  dprintf("spurious interrupt on cpu %d\n", arch_smp_get_current_cpu());
    arch_smp_ack_interrupt();
    return INT_NO_RESCHEDULE;
}

static int i386_smp_error_interrupt(void* data)
{
    // smp error interrupt
//  dprintf("smp error interrupt on cpu %d\n", arch_smp_get_current_cpu());
    arch_smp_ack_interrupt();
    return INT_NO_RESCHEDULE;
}

static unsigned int apic_read(unsigned int *addr)
{
    return *addr;
}

static void apic_write(unsigned int *addr, unsigned int data)
{
    *addr = data;
}

int arch_smp_init(kernel_args *ka)
{
    dprintf("arch_smp_init: entry\n");

    if (ka->num_cpus > 1) {
        // setup some globals
        num_cpus = ka->num_cpus;
        apic = ka->arch_args.apic;
        ioapic = ka->arch_args.ioapic;
        memcpy(cpu_apic_id, ka->arch_args.cpu_apic_id, sizeof(ka->arch_args.cpu_apic_id));
        memcpy(cpu_os_id, ka->arch_args.cpu_os_id, sizeof(ka->arch_args.cpu_os_id));
        memcpy(cpu_apic_version, ka->arch_args.cpu_apic_version, sizeof(ka->arch_args.cpu_apic_version));
        apic_timer_tics_per_sec = ka->arch_args.apic_time_cv_factor;

        // setup regions that represent the apic & ioapic
        vm_create_anonymous_region(vm_get_kernel_aspace_id(), "local_apic", (void *)&apic,
                                   REGION_ADDR_EXACT_ADDRESS, PAGE_SIZE, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);
        vm_create_anonymous_region(vm_get_kernel_aspace_id(), "ioapic", (void *)&ioapic,
                                   REGION_ADDR_EXACT_ADDRESS, PAGE_SIZE, REGION_WIRING_WIRED_ALREADY, LOCK_RW|LOCK_KERNEL);

        // set up the local apic on the boot cpu
        arch_smp_init_percpu(ka, 1);

        // set up the interrupt handlers for local apic interrupts.
        // they are mapped to absolute interrupts 0xfb-0xff, but the io handlers are all
        // base 0x20, so subtract 0x20.
        int_set_io_interrupt_handler(0xfb - 0x20, &i386_timer_interrupt, NULL, "lapic timer");
        int_set_io_interrupt_handler(0xfd - 0x20, &i386_ici_interrupt, NULL, "ici");
        int_set_io_interrupt_handler(0xfe - 0x20, &i386_smp_error_interrupt, NULL, "lapic smp error");
        int_set_io_interrupt_handler(0xff - 0x20, &i386_spurious_interrupt, NULL, "lapic spurious");
    } else {
        num_cpus = 1;
    }

    return 0;
}

static int smp_setup_apic(kernel_args *ka)
{
    unsigned int config;

    /* set spurious interrupt vector to 0xff */
    config = apic_read(APIC_SIVR) & 0xfffffc00;
    config |= APIC_ENABLE | 0xff;
    apic_write(APIC_SIVR, config);
#if 0
    /* setup LINT0 as ExtINT */
    config = (apic_read(APIC_LINT0) & 0xffff1c00);
    config |= APIC_LVT_DM_ExtINT | APIC_LVT_IIPP | APIC_LVT_TM;
    apic_write(APIC_LINT0, config);

    /* setup LINT1 as NMI */
    config = (apic_read(APIC_LINT1) & 0xffff1c00);
    config |= APIC_LVT_DM_NMI | APIC_LVT_IIPP;
    apic_write(APIC_LINT1, config);
#endif

    /* setup timer */
    config = apic_read(APIC_LVTT) & ~APIC_LVTT_MASK;
    config |= 0xfb | APIC_LVTT_M; // vector 0xfb, timer masked
    apic_write(APIC_LVTT, config);

    apic_write(APIC_ICRT, 0); // zero out the clock

    config = apic_read(APIC_TDCR) & ~0x0000000f;
    config |= APIC_TDCR_1; // clock division by 1
    apic_write(APIC_TDCR, config);

    /* setup error vector to 0xfe */
    config = (apic_read(APIC_LVT3) & 0xffffff00) | 0xfe;
    apic_write(APIC_LVT3, config);

    /* accept all interrupts */
    config = apic_read(APIC_TPRI) & 0xffffff00;
    apic_write(APIC_TPRI, config);

    config = apic_read(APIC_SIVR);
    apic_write(APIC_EOI, 0);

//  dprintf("done\n");
    return 0;
}

int arch_smp_init_percpu(kernel_args *ka, int cpu_num)
{
    // set up the local apic on the current cpu
    dprintf("arch_smp_init_percpu: setting up the apic on cpu %d", cpu_num);
    smp_setup_apic(ka);

    return 0;
}

void arch_smp_send_broadcast_ici(void)
{
    int config;

    int_disable_interrupts();

    config = apic_read(APIC_ICR1) & APIC_ICR1_WRITE_MASK;
    apic_write(APIC_ICR1, config | 0xfd | APIC_ICR1_DELMODE_FIXED | APIC_ICR1_DESTMODE_PHYS | APIC_ICR1_DEST_ALL_BUT_SELF);

    int_restore_interrupts();
}

void arch_smp_send_ici(int target_cpu)
{
    int config;

    int_disable_interrupts();

    config = apic_read(APIC_ICR2) & APIC_ICR2_MASK;
    apic_write(APIC_ICR2, config | cpu_apic_id[target_cpu] << 24);

    config = apic_read(APIC_ICR1) & APIC_ICR1_WRITE_MASK;
    apic_write(APIC_ICR1, config | 0xfd | APIC_ICR1_DELMODE_FIXED | APIC_ICR1_DESTMODE_PHYS | APIC_ICR1_DEST_FIELD);

    int_restore_interrupts();
}

void arch_smp_ack_interrupt(void)
{
    apic_write(APIC_EOI, 0);
}

#define MIN_TIMEOUT 1000

int arch_smp_set_apic_timer(bigtime_t relative_timeout, int type)
{
    unsigned int config;
    unsigned int ticks;

    if (apic == NULL)
        return -1;

    if (relative_timeout < MIN_TIMEOUT)
        relative_timeout = MIN_TIMEOUT;

    // calculation should be ok, since it's going to be 64-bit
    ticks = ((relative_timeout * apic_timer_tics_per_sec) / 1000000);

    int_disable_interrupts();

    config = apic_read(APIC_LVTT) | APIC_LVTT_M; // mask the timer
    apic_write(APIC_LVTT, config);

    apic_write(APIC_ICRT, 0); // zero out the timer

    config = apic_read(APIC_LVTT) & ~APIC_LVTT_M; // unmask the timer

    if (type == HW_TIMER_ONESHOT)
        config &= ~APIC_LVTT_TM; // clear the periodic bit
    else
        config |= APIC_LVTT_TM; // periodic

    apic_write(APIC_LVTT, config);

    dprintf("arch_smp_set_apic_timer: config 0x%x, timeout %Ld, tics/sec %d, tics %d\n",
            config, relative_timeout, apic_timer_tics_per_sec, ticks);

    apic_write(APIC_ICRT, ticks); // start it up

    int_restore_interrupts();

    return 0;
}

int arch_smp_clear_apic_timer(void)
{
    unsigned int config;

    if (apic == NULL)
        return -1;

    int_disable_interrupts();

    config = apic_read(APIC_LVTT) | APIC_LVTT_M; // mask the timer
    apic_write(APIC_LVTT, config);

    apic_write(APIC_ICRT, 0); // zero out the timer

    int_restore_interrupts();

    return 0;
}

