/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/lock.h>
#include <kernel/vm.h>
#include <kernel/fs/devfs.h>
#include <kernel/khash.h>
#include <sys/errors.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <nulibc/string.h>
#include <nulibc/stdio.h>

#include <kernel/bus/bus.h>
#include <kernel/bus/pci/pci_bus.h>

#include "pci_p.h" // private includes

struct pci_config {
	struct pci_config *next;
	char *full_path;
	struct pci_cfg *cfg;
};

struct pci_config *pci_list;

static int pci_open(dev_ident ident, dev_cookie *_cookie)
{
	struct pci_cookie *cookie;
	struct pci_config *c = (struct pci_config *)ident;

	dprintf("pci_open: entry on '%s'\n", c->full_path);

	*_cookie = c;

	return 0;
}

static int pci_freecookie(dev_cookie cookie)
{
	return 0;
}

static int pci_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static int pci_close(dev_cookie cookie)
{
	return 0;
}

static ssize_t pci_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	return ERR_NOT_ALLOWED;
}

static ssize_t pci_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	return ERR_NOT_ALLOWED;
}

static int pci_ioctl(dev_cookie _cookie, int op, void *buf, size_t len)
{
	struct pci_config *cookie = _cookie;
	int err = 0;

	switch(op) {
		case PCI_GET_CFG:
			if(len < sizeof(struct pci_cfg)) {
				err= -1;
				goto err;
			}

			err = user_memcpy(buf, cookie->cfg, sizeof(struct pci_cfg));
			break;
		case PCI_DUMP_CFG:
			dump_pci_config(cookie->cfg);
			break;
		default:
			err = ERR_INVALID_ARGS;
			goto err;
	}

err:
	return err;
}

struct dev_calls pci_hooks = {
	&pci_open,
	&pci_close,
	&pci_freecookie,
	&pci_seek,
	&pci_ioctl,
	&pci_read,
	&pci_write,
	/* cannot page from pci devices */
	NULL,
	NULL,
	NULL
};

static int pci_create_config_structs()
{
	int bus, unit, function;
	struct pci_cfg *cfg = NULL;
	struct pci_config *config;
	char char_buf[SYS_MAX_PATH_LEN];

	dprintf("pcifs_create_vnode_tree: entry\n");

	for(bus = 0; bus < 256; bus++) {
		char bus_txt[4];
		sprintf(bus_txt, "%d", bus);
		for(unit = 0; unit < 32; unit++) {
			char unit_txt[3];
			sprintf(unit_txt, "%d", unit);
			for(function = 0; function < 8; function++) {
				char func_txt[2];
				sprintf(func_txt, "%d", function);

				if(cfg == NULL)
					cfg = kmalloc(sizeof(struct pci_cfg));
				if(pci_probe(bus, unit, function, cfg) < 0) {
					// not possible for a unit to have a hole in functions
					// if we dont find one in this unit, there are no more
					break;
				}

				config = kmalloc(sizeof(struct pci_config));
				if(!config)
					panic("pci_create_config_structs: error allocating pci config struct\n");

				sprintf(char_buf, "bus/pci/%s/%s/%s/ctrl", bus_txt, unit_txt, func_txt);
				dprintf("created node '%s'\n", char_buf);

				config->full_path = kmalloc(strlen(char_buf)+1);
				strcpy(config->full_path, char_buf);

				config->cfg = cfg;
				config->next = NULL;

				config->next = pci_list;
				pci_list = config;

				cfg = NULL;
			}
		}
	}

	if(cfg != NULL)
		kfree(cfg);

	return 0;
}

int pci_bus_init(kernel_args *ka)
{
	struct pci_config *c;

	pci_list = NULL;

	pci_create_config_structs();

	// create device nodes
	for(c = pci_list; c; c = c->next) {
		devfs_publish_device(c->full_path, c, &pci_hooks);
	}

	// register with the bus manager
	bus_register_bus("/dev/bus/pci");

	return 0;
}

