/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <boot/stage2.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/sem.h>
#include <kernel/smp.h>
#include <kernel/int.h>
#include <kernel/module.h>
#include <kernel/timer.h>
#include <kernel/fs/devfs.h>
#include <string.h>
#include <newos/errors.h>

#include <kernel/bus/isa/isa.h>

#define FLOPPY_TRACE 1

#if FLOPPY_TRACE > 0
#define TRACE(x...) dprintf("floppy: " x)
#else
#define TRACE(x...)
#endif

#define SECTORS_PER_TRACK 18
#define NUM_HEADS 2
#define SECTOR_SIZE 512
#define MOTOR_TIMEOUT 10000000		// 10 seconds
#define MOTOR_SPINUP_DELAY 500000	// 500 msecs

typedef struct {
	int iobase;
	int drive_num;
	sem_id io_completion_sem;
	spinlock_t spinlock;
	isa_bus_manager *isa;
	struct timer_event motor_timer;
} floppy;

typedef struct {
	uint8 id;
	uint8 drive;
	uint8 cylinder;
	uint8 head;
	uint8 sector;
	uint8 sector_size;
	uint8 track_length;
	uint8 gap3_length;
	uint8 data_length;
} floppy_command;

typedef struct {
	uint8 st0;
	uint8 st1;
	uint8 st2;
	uint8 cylinder;
	uint8 head;
	uint8 sector;
	uint8 sector_size;
} floppy_result;

typedef enum {
	STATUS_A = 0,
	STATUS_B = 1,
	DIGITAL_OUT = 2,
	TAPE_DRIVE = 3,
	MAIN_STATUS = 4,
	DATA_RATE_SELECT = 4,
	DATA = 5,
	DIGITAL_IN = 7,
	CONFIG_CONTROL = 7
} floppy_reg_selector;

// function decls
static int floppy_detect(floppy **flp, int chain, int drive);
static int floppy_init(floppy *flp);
static void write_reg(floppy *flp, floppy_reg_selector selector, uint8 data);
static uint8 read_reg(floppy *flp, floppy_reg_selector selector);
static void turn_on_motor(floppy *flp);
static void turn_off_motor(floppy *flp);
static void wait_for_rqm(floppy *flp);
static void send_command(floppy *flp, const uint8 *data, int len);
static void read_result(floppy *flp, uint8 *data, int len);
static void recalibrate_drive(floppy *flp);
static int motor_callback(void *arg);

static int floppy_open(dev_ident ident, dev_cookie *cookie)
{
	floppy *flp = (floppy *)ident;

	*cookie = flp;

	return 0;
}

static int floppy_freecookie(dev_cookie cookie)
{
	return 0;
}

static int floppy_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
}

static int floppy_close(dev_cookie cookie)
{
	return 0;
}

static ssize_t floppy_read(dev_cookie cookie, void *buf, off_t pos, ssize_t len)
{
	floppy *flp = (floppy *)cookie;

	return ERR_NOT_ALLOWED;
}

static ssize_t floppy_write(dev_cookie cookie, const void *buf, off_t pos, ssize_t len)
{
	floppy *flp = (floppy *)cookie;

	return ERR_NOT_ALLOWED;
}

static int floppy_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	floppy *flp = (floppy *)cookie;
	int err;

	dprintf("floppy_ioctl: op %d, buf %p, len %Ld\n", op, buf, (long long)len);

	if(!flp)
		return ERR_IO_ERROR;

	switch(op) {
		default:
			err = ERR_INVALID_ARGS;
	}

	return err;
}

static struct dev_calls floppy_hooks = {
	&floppy_open,
	&floppy_close,
	&floppy_freecookie,
	&floppy_seek,
	&floppy_ioctl,
	&floppy_read,
	&floppy_write,
	/* no paging here */
	NULL,
	NULL,
	NULL
};

int dev_bootstrap(void);

int dev_bootstrap(void)
{
	floppy *flp;

	isa_bus_manager *isa;
	bool foundit = false;

	if(module_get(ISA_MODULE_NAME, 0, (void **)&isa) < 0) {
		dprintf("floppy_dev_init: no isa bus found..\n");
		return -1;
	}

	dprintf("floppy_dev_init: entry\n");

	// detect and setup the device
	if(floppy_detect(&flp, 0, 0) < 0) {
		// no floppy here
		dprintf("floppy_dev_init: no device found\n");
		return ERR_GENERAL;
	}

	flp->isa = isa;

	floppy_init(flp);

	// create device node
	devfs_publish_device("disk/floppy/0", flp, &floppy_hooks);

	{
		// test the drive
		uint8 sector[SECTOR_SIZE];
		int i;
		int ret;

		dprintf("reading sector 0...\n");
		ret = read_sector(flp, sector, 0);
		dprintf("ret %d\n", ret);
		for(i = 0; i < 512; i++) {
			dprintf("[%d] 0x%x\n", i, sector[i]);
		}


//		read_sector(flp, sector, 1);

//		read_sector(flp, sector, 2800);
//		read_sector(flp, sector, 0);
	}

	return 0;
}

static int floppy_irq_handler(void *arg)
{
	floppy *flp = (floppy *)arg;

	TRACE("irq: flp %p\n", flp);

	acquire_spinlock(&flp->spinlock);

	// see if an irq is pending
	if((read_reg(flp, STATUS_A) & 0x80) != 0) {
		// pending irq, release the sem
		TRACE("irq: was pending, releasing sem\n");
		sem_release_etc(flp->io_completion_sem, 1, SEM_FLAG_NO_RESCHED);
	}

	release_spinlock(&flp->spinlock);

	return 0;
}

static int floppy_detect(floppy **_flp, int chain, int drive)
{
	floppy *flp;

	flp = kmalloc(sizeof(floppy));
	if(!flp)
		return ERR_NO_MEMORY;

	// set up this structure
	if(chain == 0)
		flp->iobase = 0x3f0;
	else
		flp->iobase = 0x370;
	flp->drive_num = drive;

	flp->io_completion_sem = sem_create(0, "floppy io sem");
	flp->spinlock = 0;

	// set up the interrupt controller
	int_set_io_interrupt_handler(0x20 + 6, &floppy_irq_handler, flp);

	*_flp = flp;

	return 0;
}

static int floppy_init(floppy *flp)
{
	// initialize the motor timer
	timer_setup_timer(&motor_callback, flp, &flp->motor_timer);

	turn_on_motor(flp);

	// recalibrate the drive
	recalibrate_drive(flp);
}

static void write_reg(floppy *flp, floppy_reg_selector selector, uint8 data)
{
	TRACE("write to 0x%x, data 0x%x\n", flp->iobase + selector, data);

	out8(data, flp->iobase + selector);
}

static uint8 read_reg(floppy *flp, floppy_reg_selector selector)
{
	uint8 data;

	data = in8(flp->iobase + selector);
	TRACE("read from 0x%x = 0x%x\n", flp->iobase + selector, data);
	return data;
}

static int motor_callback(void *arg)
{
	floppy *flp = arg;

	turn_off_motor(flp);
	return 0;
}

static void turn_on_motor(floppy *flp)
{
	TRACE("turn_on_motor\n");

	int_disable_interrupts();

	// cancel the old timer and set it up again
	timer_cancel_event(&flp->motor_timer);
	timer_set_event(MOTOR_TIMEOUT, TIMER_MODE_ONESHOT, &flp->motor_timer);

	if((read_reg(flp, DIGITAL_OUT) & (0x10 << flp->drive_num)) == 0) {
		// it's off now, turn it on and wait
		write_reg(flp, DIGITAL_OUT, (0x10 << flp->drive_num) | 0xc);
		int_restore_interrupts();
		thread_snooze(MOTOR_SPINUP_DELAY);
		return;
	}

	int_restore_interrupts();
 }

static void turn_off_motor(floppy *flp)
{
	TRACE("turn_off_motor\n");

	write_reg(flp, DIGITAL_OUT, 0x4);
}

static void wait_for_rqm(floppy *flp)
{
	while((read_reg(flp, MAIN_STATUS) & 0x80) == 0)
		;
}

static void send_command(floppy *flp, const uint8 *data, int len)
{
	int i;

//	turn_on_motor(flp);

	for(i = 0; i < len; i++) {
		wait_for_rqm(flp);
		write_reg(flp, DATA, data[i]);
	}
}

static void read_result(floppy *flp, uint8 *data, int len)
{
	int i;

	for(i = 0; i < len; i++) {
		wait_for_rqm(flp);
		data[i] = read_reg(flp, DATA);
	}
}

static void recalibrate_drive(floppy *flp)
{
	int retry;

	TRACE("recalibrate_drive\n");

	turn_on_motor(flp);

	for(retry = 0; retry < 5; retry++) {
		uint8 command[2] = { 7, 0 }; // recalibrate command
		uint8 result[2];

		// send the recalibrate command
		send_command(flp, command, sizeof(command));
		sem_acquire(flp->io_completion_sem, 1);

		command[0] = 8;
		send_command(flp, command, 1);

		// read the result
		read_result(flp, result, sizeof(result));
		if(result[1] != 0) {
			if(result[0] & 0xc0)
				TRACE("recalibration failed\n");
			TRACE("drive is at cylinder %d, didn't make it to 0\n", result[1]);
		} else {
			// successful
			break;
		}
	}

	TRACE("recalibration successful\n");
}

static void fill_command_from_lba(floppy *flp, floppy_command *cmd, int lba)
{
	cmd->cylinder = lba / (SECTORS_PER_TRACK * NUM_HEADS);
	cmd->head = (lba / SECTORS_PER_TRACK) % NUM_HEADS;
	cmd->sector = lba % SECTORS_PER_TRACK + 1;
	cmd->drive = (flp->drive_num & 0x3) | (cmd->head << 2);
}

static size_t read_sector(floppy *flp, void *buf, int lba)
{
	floppy_command cmd;
	floppy_result res;
	int i;
	void *dma_buffer_v, *dma_buffer_p;

	TRACE("read_sector: buf %p, lba %d\n", buf, lba);

	// get a dma buffer to do work in
	flp->isa->get_dma_buffer(&dma_buffer_v, &dma_buffer_p);
	TRACE("read_sector: vbuf %p, pbuf %p\n", dma_buffer_v, dma_buffer_p);

	memset(dma_buffer_v, 0, 64*1024);

	// setup the dma engine
	flp->isa->start_floppy_dma(dma_buffer_p);

	cmd.id = 0x46; // read MFM, one head
	cmd.sector_size = 2; // 512 bytes
	cmd.track_length = SECTORS_PER_TRACK;
	cmd.gap3_length = 27; // 3.5" floppy
	cmd.data_length = 0xff;
	fill_command_from_lba(flp, &cmd, lba);

	send_command(flp, &cmd, sizeof(cmd));

	read_result(flp, &res, sizeof(res));
	if(res.st0 & 0xc0)
		return ERR_IO_ERROR;

	// copy the sector back to the passed in buffer
	memcpy(buf, dma_buffer_v, SECTOR_SIZE);

	return SECTOR_SIZE;
}

