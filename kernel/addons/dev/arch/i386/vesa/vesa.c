/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/vm.h>
#include <kernel/lock.h>
#include <kernel/fs/devfs.h>
#include <newos/drivers.h>
#include <newos/errors.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>

#include <string.h>
#include <stdio.h>

static struct vesa_info {
	int enabled;
	devfs_framebuffer_info fb_info;
	addr_range phys_memory;
} vesa;

static int vesa_open(dev_ident ident, dev_cookie *cookie)
{
	return 0;
}

static int vesa_freecookie(dev_cookie cookie)
{
	return 0;
}

static int vesa_seek(dev_cookie cookie, off_t pos, seek_type st)
{
//	dprintf("vesa_seek: entry\n");

	return ERR_NOT_ALLOWED;
}

static int vesa_close(dev_cookie cookie)
{
//	dprintf("vesa_close: entry\n");

	return 0;
}

static ssize_t vesa_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	return 0;
}

static ssize_t vesa_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	return 0;
}

static int vesa_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	int err = 0;

	if(!vesa.enabled)
		return ERR_NOT_FOUND;

	switch(op) {
		case IOCTL_DEVFS_GET_FRAMEBUFFER_INFO:
			if((addr_t)buf >= KERNEL_BASE && (addr_t)buf <= KERNEL_TOP)
				err = ERR_VM_BAD_USER_MEMORY;
			else
				err = user_memcpy(buf, &vesa.fb_info, sizeof(vesa.fb_info));
			break;
		case IOCTL_DEVFS_MAP_FRAMEBUFFER: {
			aspace_id aid = vm_get_current_user_aspace_id();
			region_id rid;
			void *address;

			if((addr_t)buf >= KERNEL_BASE && (addr_t)buf <= KERNEL_TOP) {
				err = ERR_VM_BAD_USER_MEMORY;
				goto out;
			}

			// map the framebuffer into the user's address space
			rid = vm_map_physical_memory(aid, "vesa_fb", &address, REGION_ADDR_ANY_ADDRESS,
				vesa.phys_memory.size, LOCK_RW, vesa.phys_memory.start);
			if(rid < 0) {
				err = rid;
				goto out;
			}

			// copy the new pointer back to the user
			err = user_memcpy(buf, &address, sizeof(address));
			if(err < 0) {
				vm_delete_region(aid, rid);
				goto out;
			}

			// return the region id as the return code
			err = rid;

			break;
		}
		default:
			err = ERR_INVALID_ARGS;
	}

out:
	return err;
}

static struct dev_calls vesa_hooks = {
	&vesa_open,
	&vesa_close,
	&vesa_freecookie,
	&vesa_seek,
	&vesa_ioctl,
	&vesa_read,
	&vesa_write,
	/* cannot page from /dev/vesa */
	NULL,
	NULL,
	NULL
};

int dev_bootstrap(void);

int dev_bootstrap(void)
{
	memset(&vesa, 0, sizeof(vesa));

	if(global_kernel_args.fb.enabled) {
		vesa.enabled = 1;
		vesa.fb_info.width = global_kernel_args.fb.x_size;
		vesa.fb_info.height = global_kernel_args.fb.y_size;
		vesa.fb_info.bit_depth = global_kernel_args.fb.bit_depth;
		if(global_kernel_args.fb.bit_depth == 16) {
			if(global_kernel_args.fb.red_mask_size == 5
				&& global_kernel_args.fb.green_mask_size == 6
				&& global_kernel_args.fb.blue_mask_size == 5) {
				vesa.fb_info.color_space = COLOR_SPACE_RGB565;
			} else if(global_kernel_args.fb.red_mask_size == 5
				&& global_kernel_args.fb.green_mask_size == 5
				&& global_kernel_args.fb.blue_mask_size == 5) {
				vesa.fb_info.color_space = COLOR_SPACE_RGB555;
			} else {
				vesa.fb_info.color_space = COLOR_SPACE_UNKNOWN;
			}
		} else if(global_kernel_args.fb.bit_depth == 32)
			vesa.fb_info.color_space = COLOR_SPACE_RGB888;
		else
			vesa.fb_info.color_space = COLOR_SPACE_UNKNOWN;
		vesa.phys_memory.start = global_kernel_args.fb.mapping.start;
		vesa.phys_memory.size = global_kernel_args.fb.mapping.size;

		devfs_publish_device("graphics/fb/0", NULL, &vesa_hooks);
		devfs_publish_device("graphics/vesa/fb/0", NULL, &vesa_hooks);

		return NO_ERROR;
	}

	// didn't find it
	return ERR_GENERAL;
}

