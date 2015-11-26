#include <kernel/mod_console.h>
#include <kernel/vm.h>
#include <string.h>
#include <kernel/bus/isa/isa.h>

#define SCREEN_START 0xb8000
#define SCREEN_END   0xc0000
#define LINES 25
#define COLUMNS 80

#define TEXT_INDEX 0x3d4
#define TEXT_DATA  0x3d5

#define TEXT_CURSOR_LO 0x0f
#define TEXT_CURSOR_HI 0x0e

static uint16 *gOrigin;
static isa_bus_manager *isa;

static int text_init(void)
{
    addr_t i;

    if (module_get(ISA_MODULE_NAME, 0, (void **)&isa) < 0) {
        dprintf("text module_init: no isa bus found..\n");
        return -1;
    }

    /* we always succeede, so init our stuff */
    dprintf("console/text: mapping vid mem\n");
    vm_map_physical_memory(vm_get_kernel_aspace_id(), "vid_mem", (void *)&gOrigin, REGION_ADDR_ANY_ADDRESS,
                           SCREEN_END - SCREEN_START, LOCK_RW|LOCK_KERNEL, SCREEN_START);
    dprintf("console/text: mapped vid mem to virtual address 0x%x\n", (uint32)gOrigin);

    /* pre-touch all of the memory so that we dont fault while deep inside the kernel and displaying something */
    for (i = (addr_t)gOrigin; i < (addr_t)gOrigin + (SCREEN_END - SCREEN_START); i += PAGE_SIZE) {
        uint16 val = *(volatile uint16 *)i;
        *(volatile uint16 *)i = val;
    }
    return 0;
}

static int text_uninit(void)
{
    module_put(ISA_MODULE_NAME);

    // unmap video memory (someday)
    return 0;
}

static int get_size(int *width, int *height)
{
    *width = COLUMNS;
    *height = LINES;
    return 0;
}

static int move_cursor(int x, int y)
{
    short int pos;

    if ((x < 0) || (y < 0)) pos = LINES * COLUMNS + 1;
    else pos = y * COLUMNS + x;

    isa->write_io_8(TEXT_INDEX, TEXT_CURSOR_LO);
    isa->write_io_8(TEXT_DATA, (char)pos);
    isa->write_io_8(TEXT_INDEX, TEXT_CURSOR_HI);
    isa->write_io_8(TEXT_DATA, (char)(pos >> 8));
    return 0;
}

static int put_glyph(int x, int y, uint8 glyph, uint8 attr)
{
    uint16 pair = ((uint16)attr << 8) | (uint16)glyph;
    uint16 *p = gOrigin+(y*COLUMNS)+x;
    *p = pair;
    return 0;
}

static int fill_glyph(int x, int y, int width, int height, uint8 glyph, uint8 attr)
{
    uint16 pair = ((uint16)attr << 8) | (uint16)glyph;
    int y_limit = y + height;
    while (y < y_limit) {
        uint16 *p = gOrigin+(y*COLUMNS)+x;
        uint16 *p_limit = p + width;
        while (p < p_limit) *p++ = pair;
        y++;
    }
    return 0;
}

static int blt(int srcx, int srcy, int width, int height, int destx, int desty)
{
    if ((srcx == 0) && (width == COLUMNS)) {
        // whole lines
        memmove(gOrigin + (desty * COLUMNS), gOrigin + (srcy * COLUMNS), height * COLUMNS * 2);
    } else {
        // FIXME
    }
    return 0;
}

static mod_console text_mod_console = {
    get_size,
    move_cursor,
    put_glyph,
    fill_glyph,
    blt
};

static module_header text_module_header = {
    "console/vga_text/v1",
    MODULE_CURR_VERSION,
    0,
    (void*) &text_mod_console,
    text_init,
    text_uninit
};

module_header *modules[] = {
    &text_module_header,
    0
};
