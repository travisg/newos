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
#include <kernel/vm.h>
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
#define SECTORS_PER_CYLINDER (SECTORS_PER_TRACK * NUM_HEADS)
#define SECTOR_SIZE 512
#define TRACK_SIZE (SECTOR_SIZE * SECTORS_PER_CYLINDER)
#define NUM_TRACKS 80
#define DISK_SIZE (TRACK_SIZE * NUM_TRACKS)
#define MOTOR_TIMEOUT 5000000		// 10 seconds
#define MOTOR_SPINUP_DELAY 500000	// 500 msecs

typedef struct {
	int iobase;
	int drive_num;
	sem_id io_completion_sem;
	bool io_pending;
	mutex lock;
	spinlock_t spinlock;
	isa_bus_manager *isa;
	struct timer_event motor_timer;
} floppy;

typedef struct {
	floppy *flp;
	off_t pos;
} floppy_cookie;

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
static void config_drive(floppy *flp);
static int motor_callback(void *arg);
static size_t read_sectors(floppy *flp, void *buf, int lba, int num_sectors);
static size_t write_sectors(floppy *flp, const void *buf, int lba, int num_sectors);
static size_t read_track(floppy *flp, void *buf, int lba);

static int floppy_open(dev_ident ident, dev_cookie *_cookie)
{
	floppy *flp = (floppy *)ident;
	floppy_cookie *cookie;

	cookie = (floppy_cookie *)kmalloc(sizeof(floppy_cookie));
	if(!cookie)
		return ERR_NO_MEMORY;

	cookie->flp = flp;
	cookie->pos = 0;

	*_cookie = cookie;

	return 0;
}

static int floppy_freecookie(dev_cookie _cookie)
{
	floppy_cookie *cookie = (floppy_cookie *)_cookie;

	kfree(cookie);

	return 0;
}

static int floppy_seek(dev_cookie _cookie, off_t pos, seek_type st)
{
	floppy_cookie *cookie = (floppy_cookie *)_cookie;
	int err = 0;

	TRACE("seek: pos %Ld, st %d\n", pos, st);

	mutex_lock(&cookie->flp->lock);

	switch(st) {
		case _SEEK_END:
			pos += DISK_SIZE;
			goto _SEEK_SET;
		case _SEEK_CUR:
			pos += cookie->pos;
_SEEK_SET:
		case _SEEK_SET:
			if(pos < 0)
				pos = 0;
			if(pos > DISK_SIZE)
				pos = DISK_SIZE;
			cookie->pos = pos;
			break;
		default:
			err = ERR_INVALID_ARGS;
	}

	mutex_unlock(&cookie->flp->lock);

	return err;
}

static int floppy_close(dev_cookie _cookie)
{
	return 0;
}

static ssize_t floppy_read(dev_cookie _cookie, void *_buf, off_t pos, ssize_t len)
{
	floppy_cookie *cookie = (floppy_cookie *)_cookie;
	ssize_t bytes_read = 0;
	int err;

	if(len <= 0)
		return 0;

	TRACE("read: pos %Ld, len %ld\n", pos, len);

	mutex_lock(&cookie->flp->lock);

	if(pos < 0)
		pos = cookie->pos;

	// handle partial first block
	if((pos % SECTOR_SIZE) != 0) {
		ssize_t toread;
		char buf[SECTOR_SIZE];

		err = read_sectors(cookie->flp, buf, pos / SECTOR_SIZE, 1);
		if(err <= 0)
			goto out;

		TRACE("read: 1 pos %Ld, len %ld, bytes_read %ld\n", pos, len, bytes_read);

		toread = min(len, SECTOR_SIZE);
		toread = min(toread, SECTOR_SIZE - (pos % SECTOR_SIZE));
		err = user_memcpy(_buf, buf + (pos % SECTOR_SIZE), toread);
		if(err < 0)
			goto out;
		len -= toread;
		bytes_read += toread;
		pos += toread;
	}

	// read the middle blocks
	while(len >= SECTOR_SIZE) {
		TRACE("read: 2 pos %Ld, len %ld, bytes_read %ld\n", pos, len, bytes_read);

		// try to read as many sectors as we can
		err = read_sectors(cookie->flp, (char *)_buf + bytes_read, pos / SECTOR_SIZE, len / SECTOR_SIZE);
		if(err <= 0)
			goto out;

		TRACE("write: 2 write_sectors returned %d (%d bytes)\n", err, err * SECTOR_SIZE);

		len -= err * SECTOR_SIZE;
		bytes_read += err * SECTOR_SIZE;
		pos += err * SECTOR_SIZE;
	}

	// handle partial last block
	if(len > 0 && (len % SECTOR_SIZE) != 0) {
		char buf[SECTOR_SIZE];

		err = read_sectors(cookie->flp, buf, pos / SECTOR_SIZE, 1);
		if(err <= 0)
			goto out;

		TRACE("read: 3 pos %Ld, len %ld, bytes_read %ld\n", pos, len, bytes_read);

		err = user_memcpy((char *)_buf + bytes_read, buf, len);
		if(err < 0)
			goto out;

		bytes_read += len;
		pos += len;
	}

	cookie->pos = pos;

out:
	mutex_unlock(&cookie->flp->lock);

	return bytes_read;
}

static ssize_t floppy_write(dev_cookie _cookie, const void *_buf, off_t pos, ssize_t len)
{
	floppy_cookie *cookie = (floppy_cookie *)_cookie;
	ssize_t bytes_written = 0;
	int err;

	if(len <= 0)
		return 0;

	TRACE("write: pos %Ld, len %ld\n", pos, len);

	mutex_lock(&cookie->flp->lock);

	if(pos < 0)
		pos = cookie->pos;

	// handle partial first block
	if((pos % SECTOR_SIZE) != 0) {
		ssize_t towrite;
		char buf[SECTOR_SIZE];

		/* read in a sector */
		err = read_sectors(cookie->flp, buf, pos / SECTOR_SIZE, 1);
		if(err <= 0)
			goto out;

		/* copy the modified data over */
		towrite = min(len, SECTOR_SIZE);
		towrite = min(towrite, SECTOR_SIZE - (pos % SECTOR_SIZE));
		err = user_memcpy(buf + (pos % SECTOR_SIZE), _buf, towrite);
		if(err < 0)
			goto out;

		/* write this sector back out */
		err = write_sectors(cookie->flp, buf, pos / SECTOR_SIZE, 1);
		if(err <= 0)
			goto out;

		TRACE("write: 1 pos %Ld, len %ld, bytes_written %ld\n", pos, len, bytes_written);

		len -= towrite;
		bytes_written += towrite;
		pos += towrite;
	}

	// write the middle blocks
	while(len >= SECTOR_SIZE) {
		TRACE("write: 2 pos %Ld, len %ld, bytes_written %ld\n", pos, len, bytes_written);

		// try to write as many sectors as we can
		err = write_sectors(cookie->flp, (char *)_buf + bytes_written, pos / SECTOR_SIZE, len / SECTOR_SIZE);
		if(err <= 0)
			goto out;

		TRACE("write: 2 write_sectors returned %d (%d bytes)\n", err, err * SECTOR_SIZE);

		len -= err * SECTOR_SIZE;
		bytes_written += err * SECTOR_SIZE;
		pos += err * SECTOR_SIZE;
	}

	// handle partial last block
	if(len > 0 && (len % SECTOR_SIZE) != 0) {
		char buf[SECTOR_SIZE];

		/* read in a sector */
		err = read_sectors(cookie->flp, buf, pos / SECTOR_SIZE, 1);
		if(err <= 0)
			goto out;

		/* copy the modified data over */
		err = user_memcpy(buf, (char *)_buf + bytes_written, len);
		if(err < 0)
			goto out;

		/* write the sector back out */
		err = write_sectors(cookie->flp, buf, pos / SECTOR_SIZE, 1);
		if(err <= 0)
			goto out;

		TRACE("write: 3 pos %Ld, len %ld, bytes_written %ld\n", pos, len, bytes_written);

		bytes_written += len;
		pos += len;
	}

	cookie->pos = pos;

out:
	mutex_unlock(&cookie->flp->lock);

	return bytes_written;
}

static int floppy_ioctl(dev_cookie _cookie, int op, void *buf, size_t len)
{
//	floppy_cookie *cookie = (floppy_cookie *)_cookie;
	int err;

	dprintf("floppy_ioctl: op %d, buf %p, len %ld\n", op, buf, len);

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
	devfs_publish_indexed_device("disk/floppy", flp, &floppy_hooks);

#if 0
	{
		// test the drive
		uint8 sector[SECTOR_SIZE];
		int i;
		int ret;

		/* read in a sector, print it out */
		dprintf("reading sector 0...\n");
		ret = read_sector(flp, sector, 0);
		dprintf("ret %d\n", ret);
		for(i = 0; i < 512; i++) {
			dprintf("[%d] 0x%x\n", i, sector[i]);
		}

		/* write out a sector */
		for(i = 0; i < 512; i++) {
			sector[i] = i;
		}

		dprintf("writing sector 0...\n");
		ret = write_sector(flp, sector, 0);
		dprintf("ret %d\n", ret);

//		read_sector(flp, sector, 1);

//		read_sector(flp, sector, 2800);
//		read_sector(flp, sector, 0);
	}
#endif

	return 0;
}

static int floppy_irq_handler(void *arg)
{
	floppy *flp = (floppy *)arg;

	acquire_spinlock(&flp->spinlock);

	// see if an irq is pending
	if((read_reg(flp, STATUS_A) & 0x80) != 0) {
		// pending irq, release the sem
		if(flp->io_pending) {
			TRACE("irq: %p io was pending, releasing sem\n", flp);
			flp->io_pending = false;
			sem_release_etc(flp->io_completion_sem, 1, SEM_FLAG_NO_RESCHED);
		} else {
			TRACE("irq: %p got irq, but no io was pending\n", flp);
		}
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
	mutex_init(&flp->lock, "floppy lock");
	flp->spinlock = 0;
	flp->io_pending = false;

	// set up the interrupt controller
	int_set_io_interrupt_handler(0x20 + 6, &floppy_irq_handler, flp, "floppy");

	*_flp = flp;

	return 0;
}

static int floppy_init(floppy *flp)
{
	// initialize the motor timer
	timer_setup_timer(&motor_callback, flp, &flp->motor_timer);

	turn_on_motor(flp);

	// set high speed
	write_reg(flp, DATA_RATE_SELECT, 0x0); // 500 kbps

	// recalibrate the drive
	recalibrate_drive(flp);

	// enable implicit seek
	config_drive(flp);
}

static void write_reg(floppy *flp, floppy_reg_selector selector, uint8 data)
{
//	TRACE("write to 0x%x, data 0x%x\n", flp->iobase + selector, data);

	flp->isa->write_io_8(flp->iobase + selector, data);
}

static uint8 read_reg(floppy *flp, floppy_reg_selector selector)
{
	uint8 data;

	data = flp->isa->read_io_8(flp->iobase + selector);
//	TRACE("read from 0x%x = 0x%x\n", flp->iobase + selector, data);
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

		flp->io_pending = true;

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

static void config_drive(floppy *flp)
{
	uint8 command[4];

	command[0] = 0x13; // configure command
	command[1] = 0;
	command[2] = 0x70; // Implied Seek, FIFO, Poll Disable
	command[3] = 0;

	send_command(flp, command, sizeof(command));
}

static void fill_command_from_lba(floppy *flp, floppy_command *cmd, int lba)
{
	cmd->cylinder = lba / (SECTORS_PER_TRACK * NUM_HEADS);
	cmd->head = (lba / SECTORS_PER_TRACK) % NUM_HEADS;
	cmd->sector = lba % SECTORS_PER_TRACK + 1;
	cmd->drive = (flp->drive_num & 0x3) | (cmd->head << 2);
}

static size_t read_sectors(floppy *flp, void *buf, int lba, int num_sectors)
{
	floppy_command cmd;
	floppy_result res;
	void *dma_buffer_v, *dma_buffer_p;

	TRACE("read_sectors: buf %p, lba %d, num_sectors %d\n", buf, lba, num_sectors);

	// figure out how many sectors we can really read if we do multi sector read
	num_sectors = min(num_sectors, SECTORS_PER_CYLINDER - (lba % SECTORS_PER_CYLINDER));
	TRACE("read_sectors: going to read %d sectors\n", num_sectors);

	// get a dma buffer to do work in
	flp->isa->get_dma_buffer(&dma_buffer_v, &dma_buffer_p);
	TRACE("read_sectors: vbuf %p, pbuf %p\n", dma_buffer_v, dma_buffer_p);

//	memset(dma_buffer_v, 0, 64*1024);

	// make sure the motor is on
	turn_on_motor(flp);

	// setup the dma engine
	flp->isa->start_floppy_dma(dma_buffer_p, num_sectors * SECTOR_SIZE, false);

	cmd.id = 0xc6; // multi-track, read MFM, one head
	cmd.sector_size = 2; // 512 bytes
	cmd.track_length = SECTORS_PER_TRACK;
	cmd.gap3_length = 27; // 3.5" floppy
	cmd.data_length = 0xff;
	fill_command_from_lba(flp, &cmd, lba);

	flp->io_pending = true;

	send_command(flp, (uint8 *)&cmd, sizeof(cmd));

	sem_acquire(flp->io_completion_sem, 1);

	read_result(flp, (uint8 *)&res, sizeof(res));
	if(res.st0 & 0xc0)
		return ERR_IO_ERROR;

	// copy the sector back to the passed in buffer
	memcpy(buf, dma_buffer_v, num_sectors * SECTOR_SIZE);

#if 0
	{
		int i;
		for(i = 0; i < 512; i++) {
			dprintf("[%d] 0x%x\n", i, ((char *)buf)[i]);
		}
	}
#endif

	return num_sectors;
}

static size_t write_sectors(floppy *flp, const void *buf, int lba, int num_sectors)
{
	floppy_command cmd;
	floppy_result res;
	void *dma_buffer_v, *dma_buffer_p;

	TRACE("write_sectors: buf %p, lba %d, num_sectors %d\n", buf, lba, num_sectors);

	// figure out how many sectors we can really write if we do multi sector read
	num_sectors = min(num_sectors, SECTORS_PER_CYLINDER - (lba % SECTORS_PER_CYLINDER));
	TRACE("write_sectors: going to write %d sectors\n", num_sectors);

	// get a dma buffer to do work in
	flp->isa->get_dma_buffer(&dma_buffer_v, &dma_buffer_p);
	TRACE("write_sectors: vbuf %p, pbuf %p\n", dma_buffer_v, dma_buffer_p);

//	memset(dma_buffer_v, 0, 64*1024);
	memcpy(dma_buffer_v, buf, num_sectors * SECTOR_SIZE);

	// make sure the motor is on
	turn_on_motor(flp);

	// setup the dma engine
	flp->isa->start_floppy_dma(dma_buffer_p, num_sectors * SECTOR_SIZE, true);

	cmd.id = 0xc5; // multi-track, write MFM, one head
	cmd.sector_size = 2; // 512 bytes
	cmd.track_length = SECTORS_PER_TRACK;
	cmd.gap3_length = 27; // 3.5" floppy
	cmd.data_length = 0xff;
	fill_command_from_lba(flp, &cmd, lba);

	flp->io_pending = true;

	send_command(flp, (uint8 *)&cmd, sizeof(cmd));

	sem_acquire(flp->io_completion_sem, 1);

	read_result(flp, (uint8 *)&res, sizeof(res));
	if(res.st0 & 0xc0)
		return ERR_IO_ERROR;

	return num_sectors;
}

static size_t read_track(floppy *flp, void *buf, int lba)
{
	floppy_command cmd;
	floppy_result res;
	void *dma_buffer_v, *dma_buffer_p;

	TRACE("read_track: buf %p, lba %d\n", buf, lba);

	// get a dma buffer to do work in
	flp->isa->get_dma_buffer(&dma_buffer_v, &dma_buffer_p);
	TRACE("read_track: vbuf %p, pbuf %p\n", dma_buffer_v, dma_buffer_p);

//	memset(dma_buffer_v, 0, 64*1024);

	// make sure the motor is on
	turn_on_motor(flp);

	// setup the dma engine
	flp->isa->start_floppy_dma(dma_buffer_p, TRACK_SIZE, false);

	cmd.id = 0x42; // read MFM, one track
	cmd.sector_size = 2; // 512 bytes
	cmd.track_length = SECTORS_PER_TRACK;
	cmd.gap3_length = 27; // 3.5" floppy
	cmd.data_length = 0xff;
	fill_command_from_lba(flp, &cmd, lba);

	flp->io_pending = true;

	send_command(flp, (uint8 *)&cmd, sizeof(cmd));

	sem_acquire(flp->io_completion_sem, 1);

	read_result(flp, (uint8 *)&res, sizeof(res));
	if(res.st0 & 0xc0)
		return ERR_IO_ERROR;

	// copy the sector back to the passed in buffer
	memcpy(buf, dma_buffer_v, TRACK_SIZE);

#if 0
	{
		int i;
		for(i = 0; i < 512; i++) {
			dprintf("[%d] 0x%x\n", i, ((char *)buf)[i]);
		}
	}
#endif

	return TRACK_SIZE;
}

