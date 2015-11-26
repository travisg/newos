#include <kernel/mod_console.h>
#include <kernel/vm.h>
#include <kernel/module.h>
#include <newos/errors.h>

#include <kernel/bus/pci/pci.h>

#include "svga_reg.h"
#include "vm_device_version.h"
#include "font.h"

static struct {
    addr_t      fb_base;
    addr_t      fb_phys_base;
    addr_t      fb_size;
    addr_t      fifo_base;
    addr_t      fifo_phys_base;
    addr_t      fifo_size;
    uint32      scrn_width;
    uint32      scrn_height;
    uint32      fb_offset;
    uint32      bytes_per_line;
    uint32      bits_per_pixel;
    uint32      red_mask;
    uint32      blue_mask;
    uint32      green_mask;
    int32       cursor_x;
    int32       cursor_y;
    int32       text_rows;
    int32       text_cols;
    uint32      glyph_width;
    uint32      glyph_height;
    uint32      fg, bg;
    uint32      txt_off_x, txt_off_y;
    int32       cursor_visible;
    region_id   fb_region;
    region_id   fifo_region;
    uint16      index_port;
    uint16      value_port;
    uint32      svga_id;
} vcons;

static uint8 bit_reversed[256];

static void show_cursor(int show);
static void setup_bit_reversed(void);
static void write_glyph(char c, uint32 x, uint32 y, uint32 fg, uint32 bg);
static int set_mode(uint32 width, uint32 height);
static void load_font();
static int find_and_map(void);
void out_reg(uint32 index, uint32 value);
uint32 in_reg(uint32 index);
static void init_fifo();
static void writeFIFO(uint32 value);
static void clear_screen();
static void fill_rect(uint32 color, uint32 x, uint32 y, uint32 width, uint32 height);
static void copy_rect(uint32 src_x, uint32 src_y, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height);
static void invert_rect(uint32 x, uint32 y, uint32 width, uint32 height);
static void define_font(uint32 id, uint32 width, uint32 height, uint8 *bits);
static void copy_bitmap(uint32 id, uint32 src_x, uint32 src_y, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height, uint32 fg, uint32 bg);

#define MIN(a, b) ((a)<(b)?(a):(b))

void out_reg(uint32 index, uint32 value)
{
    out32(index, vcons.index_port);
    out32(value, vcons.value_port);
}

uint32 in_reg(uint32 index)
{
    out32(index, vcons.index_port);
    return in32(vcons.value_port);
}

static void define_font(uint32 id, uint32 width, uint32 height, uint8 *bits)
{
    dprintf("define_font(%u, %u,%u, %p)\n", id, width, height, bits);
    writeFIFO(SVGA_CMD_DEFINE_BITMAP);
    writeFIFO(id);
    writeFIFO(width);
    writeFIFO(height);
    while (height--) writeFIFO(bit_reversed[*bits++]);
}

static void copy_bitmap(uint32 id, uint32 src_x, uint32 src_y, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height, uint32 fg, uint32 bg)
{
    writeFIFO(SVGA_CMD_RECT_BITMAP_COPY);
    writeFIFO(id);
    writeFIFO(src_x);
    writeFIFO(src_y);
    writeFIFO(dst_x);
    writeFIFO(dst_y);
    writeFIFO(width);
    writeFIFO(height);
    writeFIFO(fg);
    writeFIFO(bg);
}

static void write_glyph(char c, uint32 x, uint32 y, uint32 fg, uint32 bg)
{
    copy_bitmap(1, 0, vcons.glyph_height * (int)c, (x * vcons.glyph_width) + vcons.txt_off_x, (y * vcons.glyph_height) + vcons.txt_off_y, vcons.glyph_width, vcons.glyph_height, fg, bg);
}

static void load_font()
{
    vcons.glyph_width = CHAR_WIDTH;
    vcons.glyph_height = CHAR_HEIGHT;
    vcons.text_cols = vcons.scrn_width / vcons.glyph_width;
    vcons.text_rows = vcons.scrn_height / vcons.glyph_height;
    // center the text display, giving any extra pixels to the left/bottom
    vcons.txt_off_x = ((vcons.scrn_width - (vcons.text_cols * vcons.glyph_width)) + 1) / 2;
    vcons.txt_off_y = ((vcons.scrn_height - (vcons.text_rows * vcons.glyph_height)) + 0) / 2;
    define_font(1, ((CHAR_WIDTH + 7) & ~7), sizeof(FONT), FONT);
    dprintf("load_font: %dx%d glyphs\n", vcons.text_cols, vcons.text_rows);
}

static int set_mode(uint32 width, uint32 height)
{
    out_reg(SVGA_REG_WIDTH, width);
    out_reg(SVGA_REG_HEIGHT, height);
    vcons.fb_offset = in_reg(SVGA_REG_FB_OFFSET);
    vcons.bytes_per_line = in_reg(SVGA_REG_BYTES_PER_LINE);
    vcons.red_mask = in_reg(SVGA_REG_RED_MASK);
    vcons.green_mask = in_reg(SVGA_REG_GREEN_MASK);
    vcons.blue_mask = in_reg(SVGA_REG_BLUE_MASK);
    out_reg(SVGA_REG_ENABLE, 1);
    vcons.scrn_width = width;
    vcons.scrn_height = height;
    return 0;
}
static int find_and_map(void)
{
    int err;
    pci_module_hooks *pci;
    pci_info pinfo;
    aspace_id kai = vm_get_kernel_aspace_id();
    int i;
    bool foundit;

    if (module_get(PCI_BUS_MODULE_NAME, 0, (void **)&pci) < 0) {
        dprintf("vmware: no pci bus found..\n");
        err = ERR_NOT_FOUND;
        goto error0;
    }

    foundit = false;
    for (i = 0; pci->get_nth_pci_info(i, &pinfo) >= NO_ERROR; i++) {
        dprintf("vmware: looking at 0x%x:0x%x\n", pinfo.vendor_id, pinfo.device_id);
        if (pinfo.vendor_id == PCI_VENDOR_ID_VMWARE &&
                (pinfo.device_id == PCI_DEVICE_ID_VMWARE_SVGA2 || pinfo.device_id == PCI_DEVICE_ID_VMWARE_SVGA)) {
            foundit = true;
            break;
        }
    }
    if (!foundit) {
        dprintf("vmware: didn't find device on pci bus\n");
        err = ERR_NOT_FOUND;
        goto error0;
    }

    switch (pinfo.device_id) {
        case PCI_DEVICE_ID_VMWARE_SVGA:
            dprintf("vmware SVGA device detected at pci %d:%d:%d\n", pinfo.bus, pinfo.device, pinfo.function);
            vcons.index_port = SVGA_LEGACY_BASE_PORT + SVGA_INDEX_PORT * 4;
            vcons.value_port = SVGA_LEGACY_BASE_PORT + SVGA_VALUE_PORT * 4;
            break;
        case PCI_DEVICE_ID_VMWARE_SVGA2:
            dprintf("vmware SVGA2 device detected at pci %d:%d:%d\n", pinfo.bus, pinfo.device, pinfo.function);
            vcons.index_port = pinfo.u.h0.base_registers[0] + SVGA_INDEX_PORT;
            vcons.value_port = pinfo.u.h0.base_registers[0] + SVGA_VALUE_PORT;
            break;
    }
    vcons.fb_phys_base = pinfo.u.h0.base_registers[1];
    vcons.fb_size = MIN(pinfo.u.h0.base_register_sizes[1], SVGA_FB_MAX_SIZE);
    vcons.fifo_phys_base = pinfo.u.h0.base_registers[2];
    vcons.fifo_size = MIN(pinfo.u.h0.base_register_sizes[2], SVGA_MEM_SIZE);

    dprintf("vmware: index port 0x%x, value port 0x%x\n", vcons.index_port, vcons.value_port);
    dprintf("vmware: phys base 0x%x, size 0x%X\n", vcons.fb_phys_base, vcons.fb_size);
    dprintf("vmware: fifo phys base 0x%x, fifo size 0x%X\n", vcons.fifo_phys_base, vcons.fifo_size);

    vcons.fb_region = vm_map_physical_memory(kai, "vmw:fb", (void **)&vcons.fb_base, REGION_ADDR_ANY_ADDRESS, vcons.fb_size, LOCK_KERNEL | LOCK_RW, vcons.fb_phys_base);
    if (vcons.fb_region < 0) {
        err = vcons.fb_region;
        dprintf("Error mapping frame buffer: %x\n", err);
        goto error0;
    }
    vcons.fifo_region = vm_map_physical_memory(kai, "vmw:fifo", (void **)&vcons.fifo_base, REGION_ADDR_ANY_ADDRESS, vcons.fifo_size, LOCK_KERNEL | LOCK_RW, vcons.fifo_phys_base);
    if (vcons.fifo_region < 0) {
        err = vcons.fifo_region;
        dprintf("Error mapping vmw::fifo: %x\n", err);
        goto error1;
    }

    // XXX this makes the emulation unhappy (crashes vmware)
//  out_reg(SVGA_REG_ID, SVGA_ID_2);
//  vcons.svga_id = in_reg(SVGA_REG_ID);
//  dprintf("vmware: svga version %d\n", vcons.svga_id);

    vcons.bits_per_pixel = in_reg(SVGA_REG_BITS_PER_PIXEL);
    dprintf("vmware: bpp %d\n", vcons.bits_per_pixel);

    err = NO_ERROR;
    goto error0;

    // unmap vcons.fifo_region
    vm_delete_region(kai, vcons.fifo_region);
error1:
    // unmap vcons.fb_region
    vm_delete_region(kai, vcons.fb_region);
error0:
    return err;
}

static void setup_bit_reversed(void)
{
    uint8 val, rev;
    int i, j;
    for (i = 0; i < 256; i++) {
        val = i;
        rev = 0;
        for (j = 0; j < 8; j++) {
            rev <<= 1;
            if (val & 1) rev |= 1;
            val >>= 1;
        }
        bit_reversed[i] = rev;
    }
}

static void init_fifo()
{
    uint32* vmwareFIFO = (uint32*)vcons.fifo_base;
    vmwareFIFO[SVGA_FIFO_MIN] = 16;
    vmwareFIFO[SVGA_FIFO_MAX] = vcons.fifo_size;
    vmwareFIFO[SVGA_FIFO_NEXT_CMD] = 16;
    vmwareFIFO[SVGA_FIFO_STOP] = 16;
    out_reg(SVGA_REG_CONFIG_DONE, 1);
}

static void
writeFIFO(uint32 value)
{
    uint32* vmwareFIFO = (uint32*)vcons.fifo_base;

    /* Need to sync? */
    if ((vmwareFIFO[SVGA_FIFO_NEXT_CMD] + sizeof(uint32) == vmwareFIFO[SVGA_FIFO_STOP])
            || (vmwareFIFO[SVGA_FIFO_NEXT_CMD] == vmwareFIFO[SVGA_FIFO_MAX] - sizeof(uint32) &&
                vmwareFIFO[SVGA_FIFO_STOP] == vmwareFIFO[SVGA_FIFO_MIN])) {
        out_reg(SVGA_REG_SYNC, 1);
        while (in_reg(SVGA_REG_BUSY)) ;
    }
    vmwareFIFO[vmwareFIFO[SVGA_FIFO_NEXT_CMD] / sizeof(uint32)] = value;

    if (vmwareFIFO[SVGA_FIFO_NEXT_CMD] == vmwareFIFO[SVGA_FIFO_MAX] - sizeof(uint32)) {
        vmwareFIFO[SVGA_FIFO_NEXT_CMD] = vmwareFIFO[SVGA_FIFO_MIN];
    } else {
        vmwareFIFO[SVGA_FIFO_NEXT_CMD] += sizeof(uint32);
    }

//  vmwareFIFO[SVGA_FIFO_NEXT_CMD] += sizeof(uint32);
//  if (vmwareFIFO[SVGA_FIFO_NEXT_CMD] == vmwareFIFO[SVGA_FIFO_MAX])
//  {
//      vmwareFIFO[SVGA_FIFO_NEXT_CMD] = vmwareFIFO[SVGA_FIFO_MIN];
//  }
}

static void clear_screen()
{
    fill_rect(vcons.bg, 0, 0, vcons.scrn_width, vcons.scrn_height);
}

static void invert_rect(uint32 x, uint32 y, uint32 width, uint32 height)
{
#define GXinvert 0xa
    writeFIFO(SVGA_CMD_RECT_ROP_FILL);
    writeFIFO(0); // color
    writeFIFO(x);
    writeFIFO(y);
    writeFIFO(width);
    writeFIFO(height);
    writeFIFO(GXinvert);
}

static void show_cursor(int show)
{
    show = show != 0;
    if (show == vcons.cursor_visible) return;
    vcons.cursor_visible = show;
    invert_rect((vcons.cursor_x * vcons.glyph_width) + vcons.txt_off_x, (vcons.cursor_y * vcons.glyph_height) + vcons.txt_off_y, vcons.glyph_width, vcons.glyph_height);
}

static void copy_rect(uint32 src_x, uint32 src_y, uint32 dst_x, uint32 dst_y, uint32 width, uint32 height)
{
    writeFIFO(SVGA_CMD_RECT_COPY);
    writeFIFO(src_x);
    writeFIFO(src_y);
    writeFIFO(dst_x);
    writeFIFO(dst_y);
    writeFIFO(width);
    writeFIFO(height);
}

static void fill_rect(uint32 color, uint32 x, uint32 y, uint32 width, uint32 height)
{
    writeFIFO(SVGA_CMD_RECT_FILL);
    writeFIFO(color);
    writeFIFO(x);
    writeFIFO(y);
    writeFIFO(width);
    writeFIFO(height);
    //dprintf("fill_rect(%6x,%u,%u,%u,%u)\n", color, x, y, width, height);
}

uint32 colors32[16] = {
    0x00000000,     // 0 - black
    0x0000007f,     // 1 - blue
    0x00007f00,     // 2 - green
    0x00007f7f,     // 3 - cyan
    0x007f0000,     // 4 - red
    0x007f007f,     // 5 - magenta
    0x007f7f00,     // 6 - yellow
    0x007f7f7f,     // 7 - white (aka grey)
    0x00000000,     // 8 - bright? black
    0x000000ff,     // 9 - bright blue
    0x0000ff00,     // a - bright green
    0x0000ffff,     // b - bright cyan
    0x00ff0000,     // c - bright red
    0x00ff00ff,     // d - bright magenta
    0x00ffff00,     // e - bright yellow
    0x00ffffff      // f - bright? white
};
uint32 colors16[16];
uint32 *colors;

static void colors_from_attr(uint8 attr, uint32 *fg, uint32 *bg)
{
#define FMASK 0x0f
#define BMASK 0xf0
    *fg = colors[attr & FMASK];
    *bg = colors[(attr & BMASK) >> 4];
#undef FMASK
#undef BMASK
}

static int log2(uint32 v)
{
    static int table[16] = {
        0xfffffff,
        0,
        1, 1,
        2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3
    };
    int result = 0;
    dprintf("log2(%8x)", v);
    if ((v & 0xffff) == 0) { result += 16; v >>= 16; }
    if ((v & 0xff) == 0) { result += 8; v >>= 8; }
    if ((v & 0xf) == 0) { result += 4; v >>= 4; }
    result += table[v];
    dprintf(": %d\n", result);
    return result;
}

static void setup_color_table(void)
{
    if (vcons.bits_per_pixel > 16) {
        colors = colors32;
    } else {
        int index;
        int red_shift, blue_shift, green_shift;
        blue_shift = 8 - log2(vcons.blue_mask + (vcons.blue_mask & ~(vcons.blue_mask - 1)));
        green_shift = 16 - log2(vcons.green_mask + (vcons.green_mask & ~(vcons.green_mask - 1)));
        red_shift = 24 - log2(vcons.red_mask + (vcons.red_mask & ~(vcons.red_mask - 1)));
        dprintf("r: %d, g: %d, b: %d\n", red_shift, green_shift, blue_shift);
        for (index = 0; index < 16; index++) {
            uint32 base = colors32[index];
            colors16[index] =
                ((base >> red_shift) & vcons.red_mask) |
                ((base >> green_shift) & vcons.green_mask) |
                ((base >> blue_shift) & vcons.blue_mask) ;
        }
        colors = colors16;
    }
}

static int vmware_init(void)
{
    int err = find_and_map();
    if (!err) {
        vcons.cursor_visible = 0;
        vcons.bg = 0;
        vcons.fg = 0xffffff;
        setup_bit_reversed();
        init_fifo();
        set_mode(132 * CHAR_WIDTH + 4, 50 * CHAR_HEIGHT + 4);
        setup_color_table();
        clear_screen();
        dprintf("screen cleared\n");
        load_font();
        dprintf("font loaded\n");

        dprintf("vmware: fb_start 0x%x\n", in_reg(SVGA_REG_FB_START));
        dprintf("vmware: fb_offset 0x%x\n", in_reg(SVGA_REG_FB_OFFSET));
        dprintf("vmware: fb_max_size 0x%x\n", in_reg(SVGA_REG_FB_MAX_SIZE));
        dprintf("vmware: fb_size 0x%x\n", in_reg(SVGA_REG_FB_SIZE));

        dprintf("vmware: capabilities 0x%x\n", in_reg(SVGA_REG_CAPABILITIES));
        dprintf("vmware: mem_start 0x%x\n", in_reg(SVGA_REG_MEM_START));
        dprintf("vmware: mem_size 0x%x\n", in_reg(SVGA_REG_MEM_SIZE));


        // XXX pretty color pattern
        int i;
        uint32 *ptr = (uint32 *)vcons.fb_base;
        for (i=0; i < 12340; i++) {
            ptr[i] = i;
        }
    }
    return err;
}

static int vmware_uninit(void)
{
    // unmap video memory (someday)
    return 0;
}

static int get_size(int *width, int *height)
{
    *width = vcons.text_cols;
    *height = vcons.text_rows;
    return 0;
}

static int move_cursor(int x, int y)
{
    // hide the cursor before we move it
    show_cursor(0);
    if ((x >= 0) && (y >= 0)) {
        // update cursor location
        vcons.cursor_x = x;
        vcons.cursor_y = y;
        // show the cursor
        show_cursor(1);
    }

    return 0;
}

static int put_glyph(int x, int y, uint8 glyph, uint8 attr)
{
    uint32 fg, bg;
    colors_from_attr(attr, &fg, &bg);
    write_glyph(glyph, x, y, fg, bg);
    return 0;
}

static int fill_glyph(int x, int y, int width, int height, uint8 glyph, uint8 attr)
{
    uint32 fg, bg;
    int xi, yi;
    int x_lim = x + width;
    int y_lim = y + height;

    colors_from_attr(attr, &fg, &bg);

    // FIXME: see if we can't make vmware tile the bitmap for us
    for (yi = y; yi < y_lim; yi++)
        for (xi = x; xi < x_lim; xi++)
            write_glyph(glyph, xi, yi, fg, bg);
    return 0;
}

static int blt(int srcx, int srcy, int width, int height, int destx, int desty)
{
    uint32 pix_srcx = vcons.txt_off_x + (vcons.glyph_width * srcx);
    uint32 pix_srcy = vcons.txt_off_y + (vcons.glyph_height* srcy);
    uint32 pix_dstx = vcons.txt_off_x + (vcons.glyph_width * destx);
    uint32 pix_dsty = vcons.txt_off_y + (vcons.glyph_height* desty);
    copy_rect(pix_srcx, pix_srcy, pix_dstx, pix_dsty, width * vcons.glyph_width, height * vcons.glyph_height);
    return 0;
}

static mod_console vmware_mod_console = {
    get_size,
    move_cursor,
    put_glyph,
    fill_glyph,
    blt
};

static module_header vmware_module_header = {
    "console/vmware/v1",
    MODULE_CURR_VERSION,
    0,
    (void*) &vmware_mod_console,
    vmware_init,
    vmware_uninit
};

module_header *modules[] = {
    &vmware_module_header,
    0
};
