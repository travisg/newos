/* $Id$
**
** Copyright 1999 Brian J. Swetland. All rights reserved.
** Distributed under the terms of the OpenBLT License
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <i386/io.h>

#include "pci.h"

typedef struct 
{
	uchar reg:8;
	uchar func:3;
	uchar dev:5;
	uchar bus:8;
	uchar rsvd:7;
	uchar enable:1;
} confadd;

uint32 pci_read(int bus, int dev, int func, int reg, int bytes)
{
	uint32 base;
	
	union {
		confadd c;
		uint32 n;
	} u;
	
	u.c.enable = 1;
	u.c.rsvd = 0;
	u.c.bus = bus;
	u.c.dev = dev;
	u.c.func = func;
	u.c.reg = reg & 0xFC;
	
	outl(u.n,0xCF8);
	
	base = 0xCFC + (reg & 0x03);
		
	switch(bytes){
	case 1: return inb(base);
	case 2: return inw(base);
	case 4: return inl(base);
	default: return 0;
	}
}

void pci_write(int bus, int dev, int func, int reg, uint32 v, int bytes)
{
	uint32 base;
	
	union {
		confadd c;
		uint32 n;
	} u;
	
	u.c.enable = 1;
	u.c.rsvd = 0;
	u.c.bus = bus;
	u.c.dev = dev;
	u.c.func = func;
	u.c.reg = reg & 0xFC;
	
	base = 0xCFC + (reg & 0x03);
	outl(u.n,0xCF8);
	switch(bytes){
	case 1: outb(v,base); break;
	case 2: outw(v,base); break;
	case 4: outl(v,base); break;
	}
	
}

int pci_probe(int bus, int dev, int func, pci_cfg *cfg)
{
	uint32 *word = (uint32 *) cfg;
	uint32 v;	
	int i;
	for(i=0;i<4;i++){
		word[i] = pci_read(bus,dev,func,4*i,4);
	}
	if(cfg->vendor_id == 0xffff) return 1;

	cfg->bus = bus;
	cfg->dev = dev;
	cfg->func = func;
#if 0	
	printf("Device Info: /bus/pci/%d/%d/%d\n",bus,dev,func);
	printf("  * Vendor: %S   Device: %S  Class/SubClass/Interface %X/%X/%X\n",
		   cfg->vendor_id,cfg->device_id,cfg->base_class,cfg->sub_class,cfg->interface);
	printf("  * Status: %S  Command: %S  BIST/Type/Lat/CLS: %X/%X/%X/%X\n",
		   cfg->status, cfg->command, cfg->bist, cfg->header_type, 
		   cfg->latency_timer, cfg->cache_line_size);
#endif
			
	switch(cfg->header_type & 0x7F){
	case 0: /* normal device */
		for(i=0;i<6;i++){
			v = pci_read(bus,dev,func,i*4 + 0x10, 4);
			if(v) {
				int v2;
				pci_write(bus,dev,func,i*4 + 0x10, 0xffffffff, 4);
				v2 = pci_read(bus,dev,func,i*4+0x10, 4) & 0xfffffff0;
				pci_write(bus,dev,func,i*4 + 0x10, v, 4);
				v2 = 1 + ~v2;
				if(v & 1) {
//					printf("  * Base Register %d IO: %x (%x)\n",i,v&0xfff0,v2&0xffff);
					cfg->base[i] = v & 0xffff;
					cfg->size[i] = v2 & 0xffff;
				} else {
//					printf("  * Base Register %d MM: %x (%x)\n",i,v&0xfffffff0,v2);
					cfg->base[i] = v;
					cfg->size[i] = v2;
				}
			} else {
				cfg->base[i] = 0;
				cfg->size[i] = 0;
			}
			
		}
		v = pci_read(bus,dev,func,0x3c,1);
//		if((v != 0xff) && (v != 0)) printf("  * Interrupt Line: %X\n",v);
		break;
	case 1:
//		printf("  * PCI <-> PCI Bridge\n");
		break;
	default:
//		printf("  * Unknown Header Type\n");
		;
	}
	return 0;	
}

int pci_find(pci_cfg *cfg, uint16 vendor_id, uint16 device_id)
{
	int bus,dev,func;
	
	for(bus=0;bus<255;bus++){
		for(dev=0;dev<32;dev++) {
			if(pci_probe(bus,dev,0,cfg)) continue;
			if((cfg->vendor_id == vendor_id) && 
			   (cfg->device_id == device_id)) return 0;
			if(cfg->header_type & 0x80){
				for(func=1;func<8;func++){
					if(!pci_probe(bus,dev,func,cfg) &&
					   (cfg->vendor_id == vendor_id) && 
					   (cfg->device_id == device_id)) return 0;
				}
			}
		}
	}
}

