/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/vm_priv.h>
#include <kernel/arch/vm_translation_map.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <arch/cpu.h>
#include <boot/stage2.h>

static struct ppc_pteg *ptable;
static addr_t ptable_size;
static unsigned int ptable_hash_mask;
static region_id ptable_region;

// 512 MB of iospace
#define IOSPACE_SIZE (512*1024*1024)
// put it 512 MB into kernel space
#define IOSPACE_BASE (KERNEL_BASE + IOSPACE_SIZE)

#define MAX_ASIDS (PAGE_SIZE * 8)
static uint32 asid_bitmap[MAX_ASIDS / (sizeof(uint32) * 8)];
spinlock_t asid_bitmap_lock;
#define ASID_SHIFT 4
#define VADDR_TO_ASID(map, vaddr) \
    (((map)->arch_data->asid_base << ASID_SHIFT) + ((vaddr) / 0x10000000))

// vm_translation object stuff
typedef struct vm_translation_map_arch_info_struct {
    int asid_base; // shift left by ASID_SHIFT to get the base asid to use
} vm_translation_map_arch_info;

static unsigned int primary_hash(unsigned int vsid, unsigned int vaddr)
{
    unsigned int page_index;

    vsid &= 0x7ffff;
    page_index = (vaddr >> 12) & 0xffff;

    return (vsid ^ page_index) & ptable_hash_mask;
}

static unsigned int secondary_hash(unsigned int primary_hash)
{
    return ~primary_hash;
}

static int lock_tmap(vm_translation_map *map)
{
    recursive_lock_lock(&map->lock);
    return 0;
}

static int unlock_tmap(vm_translation_map *map)
{
    recursive_lock_unlock(&map->lock);
    return 0;
}

static void destroy_tmap(vm_translation_map *map)
{
    if (map->map_count > 0) {
        panic("vm_translation_map.destroy_tmap: map %p has positive map count %d\n", map, map->map_count);
    }

    // mark the asid not in use
    atomic_and(&asid_bitmap[map->arch_data->asid_base / 32], ~(1 << (map->arch_data->asid_base % 32)));

    kfree(map->arch_data);
    recursive_lock_destroy(&map->lock);
}

static int map_tmap(vm_translation_map *map, addr_t va, addr_t pa, unsigned int attributes)
{
    unsigned int hash;
    unsigned int vsid;
    struct ppc_pteg *pteg;
    struct ppc_pte *pte = NULL;
    int i;

    // lookup the vsid based off the va
    vsid = VADDR_TO_ASID(map, va);

    dprintf("vm_translation_map.map_tmap: vsid %d, pa 0x%lx, va 0x%lx\n", vsid, pa, va);

    hash = primary_hash(vsid, va);
//  dprintf("hash = 0x%x\n", hash);

    pteg = &ptable[hash];
//  dprintf("pteg @ 0x%x\n", pteg);

    // search for the first free slot for this pte
    for (i=0; i<8; i++) {
//      dprintf("trying pteg[%i]\n", i);
        if (pteg->pte[i].v == 0) {
            pteg->pte[i].hash = 0; // primary
            pte = &pteg->pte[i];
            goto found_spot;
        }
    }

    // try the secondary hash
    hash = secondary_hash(hash);

    for (i=0; i<8; i++) {
//      dprintf("trying pteg[%i]\n", i);
        if (pteg->pte[i].v == 0) {
            pteg->pte[i].hash = 1; // secondary
            pte = &pteg->pte[i];
            goto found_spot;
        }
    }

    panic("vm_translation_map.map_tmap: hash table full\n");

found_spot:
    // upper word
    pte->ppn = pa / PAGE_SIZE;
    pte->unused = 0;
    pte->r = 0;
    pte->c = 0;
    pte->wimg = 0x0;
    pte->unused1 = 0;
    if (attributes & LOCK_KERNEL)
        pte->pp = 0; // all kernel mappings are R/W to supervisor code
    else
        pte->pp = (attributes & LOCK_RW) ? 0x2 : 0x3;
    asm volatile("eieio");
    // lower word
    pte->vsid = vsid;
//  pte->hash = 0; // set up above
    pte->api = (va >> 22) & 0x3f;
    pte->v = 1;
    arch_cpu_global_TLB_invalidate();
//  dprintf("set pteg to ");
//  print_pte(&pteg->pte[i]);
//  dprintf("set pteg to 0x%x 0x%x\n", *((int *)&pteg->pte[i]), *(((int *)&pteg->pte[i])+1));

    map->map_count++;

    return 0;
}

static struct ppc_pte *lookup_pagetable_entry(vm_translation_map *map, addr_t va)
{
    unsigned int hash;
    unsigned int vsid;
    struct ppc_pteg *pteg;
    int i;

    // lookup the vsid based off the va
    vsid = VADDR_TO_ASID(map, va);

//  dprintf("vm_translation_map.lookup_pagetable_entry: vsid %d, va 0x%lx\n", vsid, va);

    hash = primary_hash(vsid, va);
//  dprintf("hash = 0x%x\n", hash);

    pteg = &ptable[hash];
//  dprintf("pteg @ 0x%x\n", pteg);

    // search for the pte
    for (i=0; i<8; i++) {
//      dprintf("trying pteg[%i]\n", i);
        if (pteg->pte[i].vsid == vsid &&
                pteg->pte[i].hash == 0 &&
                pteg->pte[i].api == ((va >> 22) & 0x3f)) {
            return &pteg->pte[i];
        }
    }

    // try the secondary hash
    hash = secondary_hash(hash);

    // search for the pte
    for (i=0; i<8; i++) {
//      dprintf("trying pteg[%i]\n", i);
        if (pteg->pte[i].vsid == vsid &&
                pteg->pte[i].hash == 1 &&
                pteg->pte[i].api == ((va >> 22) & 0x3f)) {
            return &pteg->pte[i];
        }
    }

    return NULL;
}

static int unmap_tmap(vm_translation_map *map, addr_t start, addr_t end)
{
    struct ppc_pte *pte;

    start = ROUNDOWN(start, PAGE_SIZE);
    end = ROUNDUP(end, PAGE_SIZE);

    dprintf("vm_translation_map.unmap_tmap: start 0x%lx, end 0x%lx\n", start, end);

    while (start < end) {
        pte = lookup_pagetable_entry(map, start);
        if (pte) {
            // unmap this page
            pte->v = 0;
            arch_cpu_global_TLB_invalidate();
            map->map_count--;
        }

        start += PAGE_SIZE;
    }

    return 0;
}

static int query_tmap(vm_translation_map *map, addr_t va, addr_t *out_physical, unsigned int *out_flags)
{
    struct ppc_pte *pte;

    // default the flags to not present
    *out_flags = 0;
    *out_physical = 0;

    pte = lookup_pagetable_entry(map, va);
    if (!pte)
        return NO_ERROR;

    *out_flags |= (pte->pp == 0x3) ? LOCK_RO : LOCK_RW;
    if (va >= KERNEL_BASE)
        *out_flags |= LOCK_KERNEL; // XXX is this enough?
    *out_flags |= pte->c ? PAGE_MODIFIED : 0;
    *out_flags |= pte->r ? PAGE_ACCESSED : 0;
    *out_flags |= pte->v ? PAGE_PRESENT : 0;

    *out_physical = pte->ppn * PAGE_SIZE;

    return 0;
}

static addr_t get_mapped_size_tmap(vm_translation_map *map)
{
    return map->map_count;
}

static int protect_tmap(vm_translation_map *map, addr_t base, addr_t top, unsigned int attributes)
{
    // XXX finish
    return -1;
}

static int clear_flags_tmap(vm_translation_map *map, addr_t va, unsigned int flags)
{
    struct ppc_pte *pte;

    pte = lookup_pagetable_entry(map, va);
    if (!pte)
        return NO_ERROR;

    if (flags & PAGE_MODIFIED)
        pte->c = 0;
    if (flags & PAGE_ACCESSED)
        pte->r = 0;

    arch_cpu_global_TLB_invalidate();

    return 0;
}

static void flush_tmap(vm_translation_map *map)
{
    arch_cpu_global_TLB_invalidate();
}

static int get_physical_page_tmap(addr_t pa, addr_t *va, int flags)
{
    pa = ROUNDOWN(pa, PAGE_SIZE);

    if (pa > IOSPACE_SIZE)
        panic("get_physical_page_tmap: asked for pa 0x%lx, cannot provide\n", pa);

    *va = (IOSPACE_BASE + pa);
    return 0;
}

static int put_physical_page_tmap(addr_t va)
{
    if (va < IOSPACE_BASE || va >= IOSPACE_BASE + IOSPACE_SIZE)
        panic("put_physical_page_tmap: va 0x%lx out of iospace region\n", va);

    return 0;
}

static vm_translation_map_ops tmap_ops = {
    destroy_tmap,
    lock_tmap,
    unlock_tmap,
    map_tmap,
    unmap_tmap,
    query_tmap,
    get_mapped_size_tmap,
    protect_tmap,
    clear_flags_tmap,
    flush_tmap,
    get_physical_page_tmap,
    put_physical_page_tmap
};

int vm_translation_map_create(vm_translation_map *new_map, bool kernel)
{
    // initialize the new object
    new_map->ops = &tmap_ops;
    new_map->map_count = 0;
    if (recursive_lock_create(&new_map->lock) < 0)
        return ERR_NO_MEMORY;

    new_map->arch_data = kmalloc(sizeof(vm_translation_map_arch_info));
    if (new_map->arch_data == NULL)
        return ERR_NO_MEMORY;

    int_disable_interrupts();
    acquire_spinlock(&asid_bitmap_lock);

    // allocate a ASID base for this one
    if (kernel) {
        new_map->arch_data->asid_base = 0; // set up by the bootloader
        asid_bitmap[0] |= 0x1;
    } else {
        int i;

        for (i = 0; i < MAX_ASIDS; ) {
            if (asid_bitmap[i / 32] == 0xffffffff) {
                i += 32;
                continue;
            }
            if ((asid_bitmap[i / 32] & (1 << (i % 32))) == 0) {
                // we found it
                asid_bitmap[i / 32] |= 1 << (i % 32);
                break;
            }
            i++;
        }
        if (i >= MAX_ASIDS)
            panic("vm_translation_map_create: out of ASIDS\n");
        new_map->arch_data->asid_base = i;
    }

    release_spinlock(&asid_bitmap_lock);
    int_restore_interrupts();

    return 0;
}

int vm_translation_map_module_init(kernel_args *ka)
{
    ptable = (struct ppc_pteg *)ka->arch_args.page_table_virt.start;
    ptable_size = ka->arch_args.page_table.size;
    ptable_hash_mask = ka->arch_args.page_table_mask;

    return 0;
}

void vm_translation_map_module_init_post_sem(kernel_args *ka)
{
}

int vm_translation_map_module_init2(kernel_args *ka)
{
    void *temp;

    // create a region to cover the page table
    ptable_region = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "ppc_htable", (void **)&ptable,
                    REGION_ADDR_EXACT_ADDRESS, ptable_size, REGION_WIRING_WIRED_ALREADY, LOCK_KERNEL|LOCK_RW);

    // XXX for now just map 0 - 512MB of physical memory to the iospace region
    {
        int ibats[8], dbats[8];

        getibats(ibats);
        getdbats(dbats);

        // use bat 2 & 3 to do this
        ibats[4] = dbats[4] = IOSPACE_BASE | BATU_LEN_256M | BATU_VS;
        ibats[5] = dbats[5] = 0x0 | BATL_MC | BATL_PP_RW;
        ibats[6] = dbats[6] = (IOSPACE_BASE + 256*1024*1024) | BATU_LEN_256M | BATU_VS;
        ibats[7] = dbats[7] = (256*1024*1024) | BATL_MC | BATL_PP_RW;

        setibats(ibats);
        setdbats(dbats);
    }

    // create a region to cover the iospace
    temp = (void *)IOSPACE_BASE;
    vm_create_null_region(vm_get_kernel_aspace_id(), "iospace", &temp,
                          REGION_ADDR_EXACT_ADDRESS, IOSPACE_SIZE);

    return 0;
}

// XXX horrible back door to map a page quickly regardless of translation map object, etc.
// used only during VM setup.
int vm_translation_map_quick_map(kernel_args *ka, addr_t va, addr_t pa, unsigned int attributes, addr_t (*get_free_page)(kernel_args *))
{
    unsigned int hash;
    unsigned int vsid;
    struct ppc_pteg *pteg;
    int i;

    // lookup the vsid based off the va
    vsid = getsr(va) & 0xffffff;

//  dprintf("vm_translation_map_quick_map: vsid %d, pa 0x%x, va 0x%x\n", vsid, pa, va);

    hash = primary_hash(vsid, va);
//  dprintf("hash = 0x%x\n", hash);

    pteg = &ptable[hash];
//  dprintf("pteg @ 0x%x\n", pteg);

    // search for the first free slot for this pte
    for (i=0; i<8; i++) {
//      dprintf("trying pteg[%i]\n", i);
        if (pteg->pte[i].v == 0) {
            // upper word
            pteg->pte[i].ppn = pa / PAGE_SIZE;
            pteg->pte[i].unused = 0;
            pteg->pte[i].r = 0;
            pteg->pte[i].c = 0;
            pteg->pte[i].wimg = 0x0;
            pteg->pte[i].unused1 = 0;
            pteg->pte[i].pp = 0x2; // RW
            asm volatile("eieio");
            // lower word
            pteg->pte[i].vsid = vsid;
            pteg->pte[i].hash = 0; // primary
            pteg->pte[i].api = (va >> 22) & 0x3f;
            pteg->pte[i].v = 1;
            arch_cpu_global_TLB_invalidate();
//          dprintf("set pteg to ");
//          print_pte(&pteg->pte[i]);
//          dprintf("set pteg to 0x%x 0x%x\n", *((int *)&pteg->pte[i]), *(((int *)&pteg->pte[i])+1));
            return 0;
        }
    }

    return 0;
}

// XXX currently assumes this translation map is active
static int vm_translation_map_quick_query(addr_t va, addr_t *out_physical)
{
    PANIC_UNIMPLEMENTED();
    return 0;
}

void vm_translation_map_change_asid(vm_translation_map *map)
{
// this code depends on the kernel being at 0x80000000, fix if we change that
#if KERNEL_BASE != 0x80000000
#error fix me
#endif
    int asid_base = map->arch_data->asid_base;

    asm("mtsr	0,%0" :: "g"(asid_base));
    asm("mtsr	1,%0" :: "g"(asid_base+1));
    asm("mtsr	2,%0" :: "g"(asid_base+2));
    asm("mtsr	3,%0" :: "g"(asid_base+3));
    asm("mtsr	4,%0" :: "g"(asid_base+4));
    asm("mtsr	5,%0" :: "g"(asid_base+5));
    asm("mtsr	6,%0" :: "g"(asid_base+6));
    asm("mtsr	7,%0" :: "g"(asid_base+7));
}

