/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/lock.h>

#include <string.h>

#include <kernel/bus/pci/pci_bus.h>

#include "pci_p.h" // private includes

struct pci_fs {
	fs_id id;
	mutex lock;
	void *covered_vnode;
	int root_vnode; // just a placeholder to return a pointer to
};

unsigned int pci_read_data(uint8 bus, uint8 unit, uint8 function, int reg, int bytes)
{
	struct pci_config_address addr;
	addr.enable = 1;
	addr.reserved = 0;
	addr.bus = bus;
	addr.unit = unit;
	addr.function = function;
	addr.reg = reg & 0xfc;

	out32(*(unsigned int *)&addr, CONFIG_ADDRESS);
	switch(bytes) {
		case 1:
			return in8(CONFIG_DATA + (reg & 3));
		case 2:
			return in16(CONFIG_DATA + (reg & 3));
		case 4:
			return in32(CONFIG_DATA + (reg & 3));
		default:
			return 0;
	}
}

void pci_write_data(uint8 bus, uint8 unit, uint8 function, int reg, uint32 data, int bytes)
{
	struct pci_config_address addr;
	addr.enable = 1;
	addr.reserved = 0;
	addr.bus = bus;
	addr.unit = unit;
	addr.function = function;
	addr.reg = reg & 0xfc;

	out32(*(unsigned int *)&addr, CONFIG_ADDRESS);
	switch(bytes) {
		case 1:
			out8(data, CONFIG_DATA + (reg & 3));
			break;
		case 2:
			out16(data, CONFIG_DATA + (reg & 3));
			break;
		case 4:
			out32(data, CONFIG_DATA + (reg & 3));
			break;
	}
	return;
}

static int pci_read_config(uint8 bus, uint8 unit, uint8 func, struct pci_cfg *cfg)
{
	union {
		struct pci_cfg cfg;
		uint32 word[4];
	} u;
	int i;

	for(i=0; i<4; i++) {
		u.word[i] = pci_read_data(bus, unit, func, 4*i, 4);
	}
	if(u.cfg.vendor_id == 0xffff)
		return -1;

	// move over to the passed in config structure
	memcpy(cfg, &u.cfg, sizeof(struct pci_cfg));
	cfg->bus = bus;
	cfg->unit = unit;
	cfg->func = func;

	switch(cfg->header_type & 0x7f) {
	case 0: { // normal device
		uint32 v;
		for(i=0; i<6; i++) {
			v = pci_read_data(bus, unit, func, i*4 + 0x10, 4);
			if(v) {
				int v2;
				pci_write_data(bus, unit, func, i*4 + 0x10, 0xffffffff, 4);
				v2 = pci_read_data(bus, unit, func, i*4 + 0x10, 4) & 0xfffffff0;
				pci_write_data(bus, unit, func, i*4 + 0x10, v, 4);
				v2 = 1 + ~v2;
				if(v & 1) {
					cfg->base[i] = v & 0xfff0;
					cfg->size[i] = v2 & 0xffff;
				} else {
					cfg->base[i] = v & 0xfffffff0;
					cfg->size[i] = v2;
				}
			} else {
				cfg->base[i] = 0;
				cfg->size[i] = 0;
			}
		}
		v = pci_read_data(bus, unit, func, 0x3c, 1);
		cfg->irq = (v == 0xff ? 0 : v);
		break;
	}
	case 1: //  PCI <-> PCI bridge
		break;
	default:
		break;
	}
	return 0;
}

static const char *pci_class_to_string(uint8 base_class)
{
	switch(base_class) {
		case 0: return "legacy";
		case 1: return "mass storage";
		case 2: return "network";
		case 3: return "display";
		case 4: return "multimedia";
		case 5: return "memory";
		case 6: return "bridge";
		case 7: return "communications";
		case 8: return "system";
		case 9: return "input";
		case 10: return "docking";
		case 11: return "processor";
		case 12: return "serial bus";
		case 13: return "wireless";
		case 14: return "I2O";
		case 15: return "satcom";
		case 16: return "crypto";
		case 17: return "dasp";
		default: return "unknown";
	}
}

static const char *pci_subclass_to_string(uint8 base_class, uint8 sub_class)
{
	switch(base_class) {
		case 0: // legacy
			switch(sub_class) {
				case 0: return "non-vga";
				case 1: return "vga";
			}
		case 1: // mass storage
			switch(sub_class) {
				case 0: return "scsi";
				case 1: return "ide";
				case 2: return "floppy";
				case 3: return "ipi";
				case 4: return "raid";
				case 5: return "ata";
				case 6: return "sata";
				case 0x80: return "other";
			}
			break;
		case 2: // network
			switch(sub_class) {
				case 0: return "ethernet";
				case 1: return "token ring";
				case 2: return "fddi";
				case 3: return "atm";
				case 4: return "isdn";
				case 0x80: return "other";
			}
			break;
		case 3: // video
			switch(sub_class) {
				case 0: return "vga";
				case 1: return "xga";
				case 2: return "3d";
				case 0x80: return "other";
			}
			break;
		case 4: // multimedia
			switch(sub_class) {
				case 0: return "video";
				case 1: return "audio";
				case 2: return "telephony";
				case 0x80: return "other";
			}
			break;
		case 5: // memory
			switch(sub_class) {
				case 0: return "ram";
				case 1: return "flash";
				case 0x80: return "other";
			}
			break;
		case 6: // bridge
			switch(sub_class) {
				case 0: return "host";
				case 1: return "isa";
				case 2: return "eisa";
				case 3: return "mca";
				case 4: return "pci";
				case 5: return "pcmcia";
				case 6: return "nubus";
				case 7: return "cardbus";
				case 8: return "raceway";
				case 9: return "stpci";
				case 10: return "infiniband";
				case 0x80: return "other";
			}
			break;
		case 7: // comm
			switch(sub_class) {
				case 0: return "uart";
				case 1: return "par";
				case 2: return "uart 16550a";
				case 3: return "modem";
				case 4: return "gpib";
				case 5: return "smartcard";
				case 0x80: return "other";
			}
			break;
		case 8: // base peripheral
			switch(sub_class) {
				case 0: return "pic";
				case 1: return "dma";
				case 2: return "timer";
				case 3: return "rtc";
				case 4: return "pci hotplug";
				case 0x80: return "other";
			}
		case 12: // serial bus
			switch(sub_class) {
				case 0: return "firewire";
				case 3: return "usb";
				case 4: return "fibrechannel";
			}
			break;
	}
	return "unknown";
}

void dump_pci_config(struct pci_cfg *cfg)
{
	int i;

	dprintf("dump_pci_config: dumping cfg structure at %p\n", cfg);

	dprintf("\tbus: %d, unit: %d, function: %d\n", cfg->bus, cfg->unit, cfg->func);

	dprintf("\tvendor id: %d\n", cfg->vendor_id);
	dprintf("\tdevice id: %d\n", cfg->device_id);
	dprintf("\tcommand: %d\n", cfg->command);
	dprintf("\tstatus: %d\n", cfg->status);

	dprintf("\trevision id: %d\n", cfg->revision_id);
	dprintf("\tinterface: %d\n", cfg->interface);
	dprintf("\tsub class: %d '%s'\n", cfg->sub_class, pci_subclass_to_string(cfg->base_class, cfg->sub_class));
	dprintf("\tbase class: %d '%s'\n", cfg->base_class, pci_class_to_string(cfg->base_class));

	dprintf("\tcache line size: %d\n", cfg->cache_line_size);
	dprintf("\tlatency timer: %d\n", cfg->latency_timer);
	dprintf("\theader type: %d\n", cfg->header_type);
	dprintf("\tbist: %d\n", cfg->bist);

	dprintf("\tirq: %d\n", cfg->irq);

	for(i=0; i<6; i++) {
		dprintf("\tbase[%d] = 0x%x\n", i, cfg->base[i]);
		dprintf("\tsize[%d] = 0x%x\n", i, cfg->size[i]);
	}
}

int pci_probe(uint8 bus, uint8 unit, uint8 function, struct pci_cfg *cfg)
{
	if(function > 0) {
		struct pci_cfg cfg1;

		// read info about the first unit
		if(pci_probe(bus, unit, 0, &cfg1) < 0)
			return -1;

		if(!(cfg1.header_type & 0x80)) {
			// no multiple functions for this dev
			return -1;
		}
	}

	if(pci_read_data(bus, unit, function, 0, 2) == 0xffff)
		return -1;

	if(pci_read_config(bus, unit, function, cfg) < 0)
		return -1;

	dump_pci_config(cfg);

	return 0;
}

#if 0
int pci_scan_all()
{
	int bus;
	int unit;
	int function;

	dprintf("pci_scan_all: entry\n");

	for(bus = 0; bus < 255; bus++) {
		for(unit = 0; unit < 32; unit++) {
			for(function = 0; function < 8; function++) {
				unsigned int dev_class;
				struct pci_cfg cfg;

				if(pci_read_data(bus, unit, function, 0, 2) == 0xffff)
					break;

				if(pci_read_config(bus, unit, function, &cfg) < 0)
					break;

				dump_pci_config(&cfg);

				if(!(cfg.header_type & 0x80)) {
					// no multiple functions
					break;
				}
			}
		}
	}
	return 0;
}
#endif

