/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vfs.h>
#include <kernel/debug.h>
#include <kernel/sem.h>
#include <kernel/heap.h>
#include <kernel/vm.h>
#include <kernel/arch/cpu.h>
#include <libc/string.h>
#include <libc/printf.h>
#include <dev/arch/sh4/maple/maple_bus.h>

#include "maple.h"

#define MAPLE(x) (*(volatile unsigned int *)((0xa05f6c00)+(x)))
#define DMA_BUF_SIZE (32*1024)

struct maple_bus {
	region_id dma_region;
	void *dma_buffer;
	int func_codes[4][6];
};

struct maple_fs {
	fs_id id;
	sem_id sem;
	void *covered_vnode;
	void *redir_vnode;
	int root_vnode; // just a placeholder to return a pointer to
	struct maple_bus bus;
};

struct maple_cookie {
	uint8 port;
	uint8 unit;
};

sem_id maple_sem;
static int maple_mount_count = 0;

static int maple_init(struct maple_fs *fs);

static int maple_open(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, void **_vnode, void **_cookie, struct redir_struct *redir)
{
	struct maple_fs *fs = _fs;
	int err;
	uint8 port, unit;
	struct maple_cookie *cookie;
	
	dprintf("maple_open: entry on vnode 0x%x, path = '%s'\n", _base_vnode, path);

	sem_acquire(fs->sem, 1);
	if(fs->redir_vnode != NULL) {
		// we were mounted on top of
		redir->redir = true;
		redir->vnode = fs->redir_vnode;
		redir->path = path;
		err = 0;
		goto out;		
	}		

	if(path[0] != '\0' || strlen(stream) != 2 || stream_type != STREAM_TYPE_DEVICE) {
		err = -1;
		goto err;
	}

	if(stream[0] < 'a' || stream[0] > 'd' || stream[1] < '0' || stream[1] > '5') {
		err = -1;
		goto err;
	}

	cookie = kmalloc(sizeof(struct maple_cookie));
	if(cookie == NULL) {
		err = -1;
		goto err;
	}
	
	cookie->port = stream[0] - 'a';
	cookie->unit = stream[1] - '0';
	
	*_vnode = &fs->root_vnode;	
	*_cookie = cookie;

	err = 0;

out:
err:
	sem_release(fs->sem, 1);

	return err;
}

static int maple_seek(void *_fs, void *_vnode, void *_cookie, off_t pos, seek_type seek_type)
{
	dprintf("maple_seek: entry\n");

	return -1;
}

static int maple_close(void *_fs, void *_vnode, void *_cookie)
{
	dprintf("maple_close: entry\n");

	if(_cookie != NULL)
		kfree(_cookie);
	
	return 0;
}

static int maple_create(void *_fs, void *_base_vnode, const char *path, const char *stream, stream_type stream_type, struct redir_struct *redir)
{
	struct maple_fs *fs = _fs;
	
	dprintf("maple_create: entry\n");

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

static int maple_read(void *_fs, void *_vnode, void *_cookie, void *buf, off_t pos, size_t *len)
{
	dprintf("maple_read: entry\n");

	*len = 0;
	return 0;	
}

static int maple_write(void *_fs, void *_vnode, void *_cookie, const void *buf, off_t pos, size_t *len)
{
	dprintf("maple_write: entry, len = %d\n", *len);

	*len = 0;
	return 0;
}

static int maple_ioctl(void *_fs, void *_vnode, void *_cookie, int op, void *buf, size_t len)
{
	int err = 0;
	struct maple_fs *fs = (struct maple_fs *)_fs;
	struct maple_cookie *cookie = (struct maple_cookie *)_cookie;
	
	switch(op) {
		case MAPLE_IOCTL_GET_FUNC:
			*(uint32 *)buf = fs->bus.func_codes[cookie->port][cookie->unit];
			break;
		default:
			err = -1;
	}

	return err;
}

static int maple_mount(void **fs_cookie, void *flags, void *covered_vnode, fs_id id, void **root_vnode)
{
	struct maple_fs *fs;
	int err;

	if(atomic_add(&maple_mount_count, 1) > 0) {
		err = -1;
		goto err;
	}

	fs = kmalloc(sizeof(struct maple_fs));
	if(fs == NULL) {
		err = -1;
		goto err;
	}

	fs->covered_vnode = covered_vnode;
	fs->redir_vnode = NULL;
	fs->id = id;

	{
		char temp[64];
		sprintf(temp, "maple_sem%d", id);
		
		fs->sem = sem_create(1, temp);
		if(fs->sem < 0) {
			err = -1;
			goto err1;
		}
	}

	*root_vnode = (void *)&fs->root_vnode;
	*fs_cookie = fs;

	err = maple_init(fs);
	if(err < 0)
		goto err2;
	
	return 0;

err2:
	sem_delete(fs->sem);
err1:	
	kfree(fs);
err:
	atomic_add(&maple_mount_count, -1);
	return err;
}

static int maple_unmount(void *_fs)
{
	struct maple_fs *fs = _fs;

	sem_delete(fs->sem);
	kfree(fs);

	return 0;	
}

static int maple_register_mountpoint(void *_fs, void *_v, void *redir_vnode)
{
	struct maple_fs *fs = _fs;
	
	fs->redir_vnode = redir_vnode;
	
	return 0;
}

static int maple_unregister_mountpoint(void *_fs, void *_v)
{
	struct maple_fs *fs = _fs;
	
	fs->redir_vnode = NULL;
	
	return 0;
}

static int maple_dispose_vnode(void *_fs, void *_v)
{
	return 0;
}

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

static uint32 *maple_add_trans(struct maple_fs *fs, maple_tdesc_t *tdesc, uint32 *buf)
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

static int maple_dodma(struct maple_fs *fs)
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

static int maple_docmd(struct maple_fs *fs, int8 cmd, uint8 addr, uint8 datalen, void *data, maple_frame_t *retframe)
{
	uint32 *sendbuf, *recvbuf;
	maple_tdesc_t	tdesc;
	maple_frame_t	frame;

	sendbuf = (uint32 *)fs->bus.dma_buffer;
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

	maple_add_trans(fs, &tdesc, sendbuf);

	MAPLE(0x18) = 0;
	if(maple_dodma(fs) == 0) {
		maple_read_frame(recvbuf, retframe);
		return 0;
	} 

	return -1;
}

static int maple_scan_bus(struct maple_fs *fs)
{
	int err;
	int port, unit;
	maple_frame_t frame;

	memset(&fs->bus.func_codes[0][0], 0, sizeof(fs->bus.func_codes));

	for(port = 0; port < 4; port++) {
		for(unit = 0; unit < 6; unit++) {
			err = maple_docmd(fs, MAPLE_COMMAND_DEVINFO, maple_create_addr(port, unit), 0, NULL, &frame);
			if(err < 0)
				return err;

			if(frame.cmd == MAPLE_RESPONSE_DEVINFO) {
				maple_devinfo_t *dev = (maple_devinfo_t *)frame.data;

				dev->product_name[29] = 0;

				dprintf("port %d unit %d responded to device query!\n", port, unit);
				dprintf("%c%c:  %s  (0x%x)\n", 'a'+port, '0'+unit, dev->product_name, dev->func);
				fs->bus.func_codes[port][unit] = dev->func;
			} else {
				fs->bus.func_codes[port][unit] = 0;
			}
		}
	}

	return 0;
}

static int maple_init(struct maple_fs *fs)
{
	void *foo;
	int err;
	vm_region *region;

	region = vm_create_anonymous_region(vm_get_kernel_aspace(), "maple_bus_dma_region", &foo,
		REGION_ADDR_ANY_ADDRESS, DMA_BUF_SIZE, REGION_WIRING_WIRED_CONTIG, LOCK_RW|LOCK_KERNEL);
	fs->bus.dma_region = region->id;

	vm_get_page_mapping(vm_get_kernel_aspace(), (addr)foo, (addr *)&fs->bus.dma_buffer);
	fs->bus.dma_buffer = (void *)PHYS_ADDR_TO_P2(fs->bus.dma_buffer);
	memset(fs->bus.dma_buffer, 0, DMA_BUF_SIZE);
	
	dprintf("maple_init: created dma area at P2 addr 0x%x\n", fs->bus.dma_buffer);
	
	/* reset hardware */
	MAPLE(0x8c) = 0x6155404f;
	MAPLE(0x10) = 0;
	MAPLE(0x80) = (50000 << 16) | 0; // set the timeout and bitrate to 2Mbps

	/* enable the bus */
	MAPLE(0x14) = 1;

	maple_scan_bus(fs);

	return 0;

error1:
//	vm_delete_area(fs->bus.dma_area);
error:
	return err;
}

struct fs_calls maple_hooks = {
	&maple_mount,
	&maple_unmount,
	&maple_register_mountpoint,
	&maple_unregister_mountpoint,
	&maple_dispose_vnode,
	&maple_open,
	&maple_seek,
	&maple_read,
	&maple_write,
	&maple_ioctl,
	&maple_close,
	&maple_create,
};

int maple_bus_init(kernel_args *ka)
{
	// create device node
	vfs_register_filesystem("maple_bus_fs", &maple_hooks);
	sys_create("/bus", "", STREAM_TYPE_DIR);
	sys_create("/bus/maple", "", STREAM_TYPE_DIR);
	sys_mount("/bus/maple", "maple_bus_fs");

	return 0;
}

