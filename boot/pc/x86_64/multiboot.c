#include <stdio.h>
#include "stage2_priv.h"
#include "multiboot.h"

static uint32 read32(const void *ptr, unsigned int offset)
{
    return *(const uint32 *)((const char *)ptr + offset);
}

static void dump_multiboot_mmap(const void *mmap_base, uint32 len)
{
    unsigned int index = 0;
    struct multiboot_mmap *mmap = (struct multiboot_mmap *)mmap_base;

    while (index < (len / sizeof(struct multiboot_mmap))) {
//      dprintf("\t\tsize 0x%x\n", mmap[index].size);
        uint64 addr = ((uint64)mmap[index].base_addr_high << 32) | mmap[index].base_addr_low;
        dprintf("\t\taddr 0x%Lx, ", addr);
        uint64 length = ((uint64)mmap[index].len_high << 32) | mmap[index].len_low;
        dprintf("len 0x%Lx, ", length);
        dprintf("type 0x%x\n", mmap[index].type);

        index++;
    }
}

void dump_multiboot(const void *multiboot)
{
    uint32 flags = read32(multiboot, 0);

    dprintf("multiboot struct at %p\n", multiboot);
    dprintf("\tflags 0x%x\n", flags);

    if (flags & (1<<0)) {
        dprintf("\tmem_lower 0x%x\n", read32(multiboot, 4));
        dprintf("\tmem_upper 0x%x\n", read32(multiboot, 8));
    }
    if (flags & (1<<1)) {
        dprintf("\tboot_device 0x%x\n", read32(multiboot, 12));
    }
    if (flags & (1<<2)) {
        dprintf("\tcmdline 0x%x\n", read32(multiboot, 16));
    }
    if (flags & (1<<3)) {
        dprintf("\tmods_count 0x%x\n", read32(multiboot, 20));
        dprintf("\tmods_addr 0x%x\n", read32(multiboot, 24));
    }
    if (flags & (1<<6)) {
        dprintf("\tmmap_length 0x%x\n", read32(multiboot, 44));
        dprintf("\tmmap_addr 0x%x\n", read32(multiboot, 48));

        dump_multiboot_mmap((void *)(unsigned long)read32(multiboot, 48), read32(multiboot, 44));
    }
}

void fill_ka_memranges(const void *multiboot)
{
    uint32 flags;

    flags = read32(multiboot, 0);

    if (flags & (1<<6)) {
        unsigned int index = 0;
        unsigned int len = read32(multiboot, 44);
        struct multiboot_mmap *mmap = (struct multiboot_mmap *)(addr_t)read32(multiboot, 48);

        ka->num_phys_mem_ranges = 0;

        while (index < (len / sizeof(struct multiboot_mmap))) {
            if (mmap[index].type == 1) {
                uint64 addr = ((uint64)mmap[index].base_addr_high << 32) | mmap[index].base_addr_low;
                uint64 length = ((uint64)mmap[index].len_high << 32) | mmap[index].len_low;

                ka->phys_mem_range[ka->num_phys_mem_ranges].start = addr;
                ka->phys_mem_range[ka->num_phys_mem_ranges].size = length;
                ka->num_phys_mem_ranges++;
            }

            index++;
        }
    } else {
        panic("no multiboot mmap\n");
    }
}

