#include <kernel/kernel.h>
#include <kernel/console.h>
#include <kernel/debug.h>
#include <kernel/vm.h>
#include <kernel/int.h>
#include <kernel/sem.h>
#include <kernel/vfs.h>

#include <arch_cpu.h>
#include <arch_int.h>

#include <string.h>
#include <printf.h>

#include "pci_bus.h"

#define CONFIG_ADDRESS 0xcf8
#define CONFIG_DATA 0xcfc

struct pci_config_address {
	unsigned reg : 8,
		function : 3,
		unit : 5,
		bus : 8,
		reserved : 7,
		enable : 1;
} _PACKED;

struct pci_cfg {
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
};

struct pci_fs {
	fs_id id;
	sem_id sem;
	void *covered_vnode;
	void *redir_vnode;
	int root_vnode; // just a placeholder to return a pointer to
};

sem_id pci_sem;

static int pci_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct pci_fs *fs = _fs;
	int err;
	
	dprintf("pci_open: entry on vnode 0x%x, path = '%s'\n", _base_vnode, path);

	sem_acquire(fs->sem, 1);
	if(fs->redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = fs->redir_vnode;
		redir->path = path;
		err = 0;
		goto out;		
	}		

	if(path[0] != '\0' || stream[0] != '\0' || stream_type != STREAM_TYPE_DEVICE) {
		err = -1;
		goto err;
	}
	
	*_vnode = &fs->root_vnode;	
	*_cookie = NULL;

	err = 0;

out:
err:
	sem_release(fs->sem, 1);

	return err;
}

static int pci_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
	dprintf("pci_seek: entry\n");

	return -1;
}

static int pci_close(void *_fs, void *_vnode, void *_cookie)
{
	dprintf("pci_close: entry\n");

	return 0;
}

static int pci_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct pci_fs *fs = _fs;
	
	dprintf("pci_create: entry\n");

	sem_acquire(fs->sem, 1);
	
	if(fs->redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = fs->redir_vnode;
		redir->path = path;
		sem_release(fs->sem, 1);
		return 0;
	}
	sem_release(fs->sem, 1);
	
	return -1;
}

static int pci_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	dprintf("pci_read: entry\n");

	*len = 0;
	return 0;	
}

static int pci_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	dprintf("pci_write: entry, len = %d\n", *len);

	*len = 0;
	return 0;
}

static unsigned int pci_read_data(int bus, int unit, int function, int reg, int bytes)
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

static int pci_read_config(int bus, int unit, int func, struct pci_cfg *cfg)
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
	memcpy(cfg, &u.cfg, sizeof(struct pci_cfg));
	return 0;
}

static void dump_pci_config(struct pci_cfg *cfg)
{
	dprintf("dump_pci_config: dumping cfg structure at 0x%x\n", cfg);
	
	dprintf("\tvendor id: %d\n", cfg->vendor_id);
	dprintf("\tdevice id: %d\n", cfg->device_id);
	dprintf("\tcommand: %d\n", cfg->command);
	dprintf("\tstatus: %d\n", cfg->status);

	dprintf("\trevision id: %d\n", cfg->revision_id);
	dprintf("\tinterface: %d\n", cfg->interface);
	dprintf("\tsub class: %d\n", cfg->sub_class);
	dprintf("\tbase class: %d\n", cfg->base_class);

	dprintf("\tcache line size: %d\n", cfg->cache_line_size);
	dprintf("\tlatency timer: %d\n", cfg->latency_timer);
	dprintf("\theader type: %d\n", cfg->header_type);
	dprintf("\tbist: %d\n", cfg->bist);
}

static int pci_scan_all()
{
	int bus;
	int unit;
	
	dprintf("pci_scan_all: entry\n");
	
	for(bus = 0; bus < 255; bus++) {
		for(unit = 0; unit < 32; unit++) {
			unsigned int dev_class;
			struct pci_cfg cfg;
						
			if(pci_read_data(bus, unit, 0, 0, 2) == 0xffff)
				continue;

			if(pci_read_config(bus, unit, 0, &cfg) < 0)
				continue;

			dump_pci_config(&cfg);
		}
	}
	return 0;
}

static int pci_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	int err = 0;
	
	switch(op) {
		// XXX hack
		case 99:
			// get config info
			pci_scan_all();
			break;
		default:
			err = -1;
	}

	return err;
}

static int pci_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct pci_fs *fs;
	int err;

	fs = kmalloc(sizeof(struct pci_fs));
	if(fs == NULL) {
		err = -1;
		goto err;
	}

	fs->covered_vnode = covered_vnode;
	fs->redir_vnode = NULL;
	fs->id = id;

	{
		char temp[64];
		sprintf(temp, "pci_sem%d", id);
		
		fs->sem = sem_create(1, temp);
		if(fs->sem < 0) {
			err = -1;
			goto err1;
		}
	}

	*root_vnode = (void *)&fs->root_vnode;
	*fs_cookie = fs;

	return 0;

err1:	
	kfree(fs);
err:
	return err;
}

static int pci_unmount(void *_fs)
{
	struct pci_fs *fs = _fs;

	sem_delete(fs->sem);
	kfree(fs);

	return 0;	
}

static int pci_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct pci_fs *fs = _fs;
	
	fs->redir_vnode = redir_vnode;
	
	return 0;
}

static int pci_unregister_mountpoint(void *_fs, void *_v)
{
	struct pci_fs *fs = _fs;
	
	fs->redir_vnode = NULL;
	
	return 0;
}

static int pci_dispose_vnode(void *_fs, void *_v)
{
	return 0;
}

struct fs_calls pci_hooks = {
	&pci_mount,
	&pci_unmount,
	&pci_register_mountpoint,
	&pci_unregister_mountpoint,
	&pci_dispose_vnode,
	&pci_open,
	&pci_seek,
	&pci_read,
	&pci_write,
	&pci_ioctl,
	&pci_close,
	&pci_create,
};

int pci_bus_init(kernel_args *ka)
{
	// create device node
	vfs_register_filesystem("pci_bus_fs", &pci_hooks);
	vfs_create(NULL, "/bus", "", STREAM_TYPE_DIR);
	vfs_create(NULL, "/bus/pci", "", STREAM_TYPE_DIR);
	vfs_mount("/bus/pci", "pci_bus_fs");

	return 0;
}

