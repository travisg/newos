/* $Id$
**
** Copyright 1999 Brian J. Swetland. All rights reserved.
** Distributed under the terms of the OpenBLT License
*/

#ifndef _PCI_H
#define _PCI_H

#include <blt/types.h>

typedef struct 
{
	/* normal header stuff */
	uint16 vendor_id;
	uint16 device_id;
	
	uint16 command;
	uint16 status;
	
	uint8 revision_id;
	uint8 interface;
	uint8 sub_class;
	uint8 base_class;
	
	uint8 cache_line_size;
	uint8 latency_timer;
	uint8 header_type;
	uint8 bist;	
	
	/* device info */
	uint8 bus;
	uint8 dev;
	uint8 func;
	uint8 _pad;

	/* base registers */	
	uint32 base[6];
	uint32 size[6];
	
} pci_cfg;

int pci_find(pci_cfg *cfg, uint16 vendor_id, uint16 device_id);

uint32 pci_read(int bus, int dev, int func, int reg, int bytes);
void pci_write(int bus, int dev, int func, int reg, uint32 v, int bytes);


#endif
