/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>
#include <kernel/sem.h>
#include <kernel/lock.h>
#include <kernel/heap.h>
#include <kernel/vm.h>
#include <kernel/arch/cpu.h>
#include <kernel/fs/devfs.h>
#include <nulibc/string.h>
#include <nulibc/stdio.h>
#include <kernel/dev/arch/sh4/maple/maple_bus.h>

#define MAPLE(x) (*(volatile unsigned int *)((0xa05f6c00)+(x)))
#define DMA_BUF_SIZE (128*1024)

struct maple_config {
	struct maple_config *next;
	char *full_path;
	maple_devinfo_t cfg;
	uint8 port;
	uint8 unit;
};

struct maple_bus {
	region_id dma_region_id;
	void *dma_buffer;
	int func_codes[4][6];
	struct maple_config *maple_list;
	mutex lock;
} mbus;

static uint8 maple_create_addr(uint8 port, uint8 unit)
{
	uint8 addr;

	if(port > 3 || unit > 5)
		return -1;

	addr = port << 6;
	if(unit != 0) {
		addr |= (1 << (unit - 1)) & 0x1f;
	} else {
		addr |= 0x20;
	}

	return addr;
}

static uint32 *maple_add_trans(struct maple_bus *bus, maple_tdesc_t *tdesc, uint32 *buf)
{
	int i;

	// build the transfer descriptor's two words
	*buf++ = tdesc->length | (tdesc->port << 16) | (tdesc->lastdesc << 31);
	*buf++ = ((uint32)tdesc->recvaddr) & 0xfffffff;

	// do each of the frames in this transfer
	for(i=0; i<tdesc->numframes; i++) {
		maple_frame_t *frame = &tdesc->frames[i];

		*buf++ = (frame->cmd & 0xff) | (frame->to << 8) |
			(frame->from << 16) | (frame->datalen << 24);
		/* copy data over */
		if(frame->datalen > 0) {
			memcpy(buf, frame->data, frame->datalen * 4);
			buf += frame->datalen;
		}
	}

	return buf;
}

static int maple_dodma(struct maple_bus *bus)
{
	// start the dma
	MAPLE(0x18) = 1;

	// wait for the dma to be done
	while(MAPLE(0x18) & 0x1)
		;

	return 0;
}

static void maple_read_frame(uint32 *_buf, maple_frame_t *frame)
{
	uint8	*buf = (uint8 *)_buf;

	frame->cmd = buf[0];
	frame->to = buf[1];
	frame->from = buf[2];
	frame->datalen = buf[3];
	frame->data = &buf[4];
}

static int maple_docmd(struct maple_bus *bus, int8 cmd, uint8 addr, uint8 datalen, void *data, maple_frame_t *retframe)
{
	uint32 *sendbuf, *recvbuf;
	maple_tdesc_t	tdesc;
	maple_frame_t	frame;

	sendbuf = (uint32 *)bus->dma_buffer;
	recvbuf = sendbuf + 256;

	// build the transfer descriptor with one frame in it
	tdesc.lastdesc = 1;
	tdesc.port = addr >> 6;
	tdesc.length = datalen;
	tdesc.recvaddr = recvbuf;
	tdesc.numframes = 1;
	tdesc.frames = &frame;
	frame.cmd = cmd;
	frame.data = data;
	frame.datalen = datalen;
	frame.from = tdesc.port << 6;
	frame.to = addr;

	// set the dma ptr
	MAPLE(0x04) = ((uint32)sendbuf) & 0xfffffff;

	maple_add_trans(bus, &tdesc, sendbuf);

	MAPLE(0x18) = 0;
	if(maple_dodma(bus) == 0) {
		maple_read_frame(recvbuf, retframe);
		return 0;
	}

	return -1;
}



static int maple_open(dev_ident ident, dev_cookie *_cookie)
{
	struct maple_config *c = (struct maple_config *)ident;

	dprintf("maple_open: entry on '%s'\n", c->full_path);

	*_cookie = c;

	return 0;
}

static int maple_freecookie(dev_cookie _cookie)
{
	return 0;
}

static int maple_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	dprintf("maple_seek: entry\n");

	return ERR_NOT_ALLOWED;
}

static int maple_close(dev_cookie _cookie)
{
	dprintf("maple_close: entry\n");

	return 0;
}

static int maple_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	dprintf("maple_read: entry\n");

	return ERR_NOT_ALLOWED;
}

static int maple_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	dprintf("maple_write: entry, len = %d\n", len);

	return ERR_NOT_ALLOWED;
}

static int maple_ioctl(dev_cookie _cookie, int op, void *buf, size_t len)
{
	int err = 0;
	struct maple_config *cookie = (struct maple_config *)_cookie;

	switch(op) {
		case MAPLE_IOCTL_GET_FUNC:
			*(uint32 *)buf = mbus.func_codes[cookie->port][cookie->unit];
			break;
		case MAPLE_IOCTL_SEND_COMMAND: {
			maple_command *cmd = buf;
			int8 address = maple_create_addr(cookie->port, cookie->unit);

			mutex_lock(&mbus.lock);

			err = maple_docmd(&mbus, cmd->cmd, address, cmd->outdatalen, cmd->outdata, cmd->retframe);
			if(err == NO_ERROR) {
				// copy the data back out
				memcpy(cmd->indata, cmd->retframe->data, min(cmd->retframe->datalen * 4, cmd->indatalen * 4));
			}
			mutex_unlock(&mbus.lock);
			break;
		}
		default:
			err = -1;
	}

	return err;
}

static int maple_scan_bus(struct maple_bus *bus)
{
	int err;
	int port, unit;
	maple_frame_t frame;

	memset(&bus->func_codes[0][0], 0, sizeof(bus->func_codes));

	for(port = 0; port < 4; port++) {
		for(unit = 0; unit < 6; unit++) {
			err = maple_docmd(bus, MAPLE_COMMAND_DEVINFO, maple_create_addr(port, unit), 0, NULL, &frame);
			if(err < 0)
				return err;

			if(frame.cmd == MAPLE_RESPONSE_DEVINFO) {
				struct maple_config *cfg;
				char char_buf[SYS_MAX_PATH_LEN];

				cfg = (struct maple_config *)kmalloc(sizeof(struct maple_config));
				if(!cfg)
					return ERR_NO_MEMORY;

				memcpy(&cfg->cfg, frame.data, sizeof(cfg->cfg));

				cfg->cfg.product_name[29] = 0;

				dprintf("port %d unit %d responded to device query!\n", port, unit);
				dprintf("%c%c:  %s  (0x%x)\n", 'a'+port, '0'+unit, cfg->cfg.product_name, cfg->cfg.func);
				bus->func_codes[port][unit] = cfg->cfg.func;
				cfg->port = port;
				cfg->unit = unit;

				sprintf(char_buf, "bus/maple/%d/%d/ctrl", port, unit);
				cfg->full_path = kmalloc(strlen(char_buf)+1);
				strcpy(cfg->full_path, char_buf);

				cfg->next = bus->maple_list;
				bus->maple_list = cfg;
			} else {
				bus->func_codes[port][unit] = 0;
			}
		}
	}

	return 0;
}

static int maple_init(struct maple_bus *bus)
{
	void *foo = NULL;
	int err;

	bus->dma_region_id = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "maple_bus_dma_region", &foo,
		REGION_ADDR_ANY_ADDRESS, DMA_BUF_SIZE, REGION_WIRING_WIRED_CONTIG, LOCK_RW|LOCK_KERNEL);
	if(bus->dma_region_id < 0)
		panic("could not create maple bus dma region\n");

	mutex_init(&bus->lock, "maple_bus_lock");

	vm_get_page_mapping(vm_get_kernel_aspace_id(), (addr)foo, (addr *)&bus->dma_buffer);
	bus->dma_buffer = (void *)PHYS_ADDR_TO_P2(bus->dma_buffer);
	memset(bus->dma_buffer, 0, DMA_BUF_SIZE);

	dprintf("maple_init: created dma area at P2 addr 0x%x\n", bus->dma_buffer);

	/* reset hardware */
	MAPLE(0x8c) = 0x6155404f;
	MAPLE(0x10) = 0;
	MAPLE(0x80) = (50000 << 16) | 0; // set the timeout and bitrate to 2Mbps

	/* enable the bus */
	MAPLE(0x14) = 1;

	return 0;

error1:
//	vm_delete_area(fs->bus.dma_area);
error:
	return err;
}

struct dev_calls maple_hooks = {
	&maple_open,
	&maple_close,
	&maple_freecookie,
	&maple_seek,
	&maple_ioctl,
	&maple_read,
	&maple_write,
	/* cannot page from maple devices */
	NULL,
	NULL,
	NULL
};

int maple_bus_init(kernel_args *ka)
{
	struct maple_config *c;

	memset(&mbus, 0, sizeof(mbus));
	maple_init(&mbus);
	maple_scan_bus(&mbus);

	// create device nodes
	for(c = mbus.maple_list; c; c = c->next) {
		devfs_publish_device(c->full_path, c, &maple_hooks);
	}
	return 0;
}

