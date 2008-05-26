#ifndef __MULTIBOOT_H
#define __MULTIBOOT_H

struct multiboot_info {
	uint32 flags;

	uint32 mem_lower;
	uint32 mem_upper;

	uint32 boot_device;

	uint32 cmdline;

	uint32 mods_count;
	uint32 mods_addr;

	uint32 syms[4];

	uint32 mmap_length;
	uint32 mmap_addr;
	
	uint32 drives_length;
	uint32 drives_addr;

	uint32 config_table;

	uint32 boot_loader_name;

	uint32 apm_table;

	uint32 vbe_control_info;
	uint32 vbe_mode_info;
	uint32 vbe_mode;
	uint32 vbe_interface_seg;
	uint32 vbe_interface_off;
	uint32 vbe_interface_len;
};

struct multiboot_mmap {
	uint32 size;
	uint32 base_addr_low;
	uint32 base_addr_high;
	uint32 len_low;
	uint32 len_high;
	uint32 type;
};

void dump_multiboot(const void *multiboot);

#endif

