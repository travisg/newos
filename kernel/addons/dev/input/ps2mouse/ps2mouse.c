/*
 * ps2mouse.c:
 * PS/2 mouse device driver for NewOS and OpenBeOS.
 * Author:     Elad Lahav (elad@eldarshany.com)
 * Created:    21.12.2001
 * Modified:   11.1.2002
 */

/*
 * A PS/2 mouse is connected to the IBM 8042 controller, and gets its
 * name from the IBM PS/2 personal computer, which was the first to
 * use this device. All resources are shared between the keyboard, and
 * the mouse, referred to as the "Auxiliary Device".
 * I/O:
 * ~~~
 * The controller has 3 I/O registers:
 * 1. Status (input), mapped to port 64h
 * 2. Control (output), mapped to port 64h
 * 3. Data (input/output), mapped to port 60h
 * Data:
 * ~~~~
 * Since a mouse is an input only device, data can only be read, and
 * not written. A packet read from the mouse data port is composed of
 * three bytes:
 * byte 0: status byte, where
 * - bit 0: Y overflow (1 = true)
 * - bit 1: X overflow (1 = true)
 * - bit 2: MSB of Y offset
 * - bit 3: MSB of X offset
 * - bit 4: Syncronization bit (always 1)
 * - bit 5: Middle button (1 = down)
 * - bit 6: Right button (1 = down)
 * - bit 7: Left button (1 = down)
 * byte 1: X position change, since last probed (-127 to +127)
 * byte 2: Y position change, since last probed (-127 to +127)
 * Interrupts:
 * ~~~~~~~~~~
 * The PS/2 mouse device is connected to interrupt 12, which means that
 * it uses the second interrupt controller (handles INT8 to INT15). In
 * order for this interrupt to be enabled, both the 5th interrupt of
 * the second controller AND the 3rd interrupt of the first controller
 * (cascade mode) should be unmasked.
 * The controller uses 3 consecutive interrupts to inform the computer
 * that it has new data. On the first the data register holds the status
 * byte, on the second the X offset, and on the 3rd the Y offset.
 */

#include <kernel/kernel.h>
#include <kernel/heap.h>
#include <kernel/int.h>
#include <kernel/debug.h>
#include <kernel/fs/devfs.h>
#include <kernel/arch/int.h>
#include <kernel/sem.h>
#include <kernel/module.h>
#include <string.h>
#include <kernel/bus/isa/isa.h>

/////////////////////////////////////////////////////////////////////////
// definitions

// I/O addresses
#define PS2_PORT_DATA            0x60
#define PS2_PORT_CTRL            0x64

// control words
#define PS2_CTRL_WRITE_CMD       0x60
#define PS2_CTRL_WRITE_AUX       0xD4

// data words
#define PS2_CMD_DEV_INIT         0x43
#define PS2_CMD_ENABLE_MOUSE     0xF4
#define PS2_CMD_DISABLE_MOUSE    0xF5
#define PS2_CMD_RESET_MOUSE      0xFF

#define PS2_RES_ACK              0xFA
#define PS2_RES_RESEND           0xFE

// interrupts
#define INT_PS2_MOUSE            0x0C
#define INT_CASCADE              0x02

#define PACKET_SIZE              3

/*
 * mouse_data:
 * Holds the data read from the PS/2 auxiliary device.
 */
typedef struct {
   char status;
	char delta_x;
	char delta_y;
} mouse_data; // mouse_data

static mouse_data md_int;
static mouse_data md_read;
static sem_id mouse_sem;
static bool in_read;
static isa_bus_manager *isa;

/////////////////////////////////////////////////////////////////////////
// interrupt

/*
 * handle_mouse_interrupt:
 * Interrupt handler for the mouse device. Called whenever the I/O
 * controller generates an interrupt for the PS/2 mouse. Reads mouse
 * information from the data port, and stores it, so it can be accessed
 * by read() operations. The full data is obtained using 3 consecutive
 * calls to the handler, each holds a different byte on the data port.
 * Parameters:
 * void*, ignored
 * Return value:
 * int, ???
 */
static int handle_mouse_interrupt(void* data)
{
   char c;
	static int next_input = 0;

	// read port
   c = isa->read_io_8(PS2_PORT_DATA);

	// put port contents in the appropriate data member, according to
	// current cycle
	switch(next_input) {
   // status byte
	case 0:
	   md_int.status = c;
		break;

	// x-axis change
   case 1:
      md_int.delta_x += c;
		break;

	// y-axis change
	case 2:
      md_int.delta_y += c;

		// check if someone is waiting to read data
		if(in_read) {
		   // copy data to read structure, and release waiting process
         memcpy(&md_read, &md_int, sizeof(mouse_data));
			memset(&md_int, 0, sizeof(mouse_data));
			in_read = false;
	      sem_release_etc(mouse_sem, 1, SEM_FLAG_NO_RESCHED);
		} // if
		break;
	} // switch

   next_input = (next_input + 1) % 3;
	return INT_NO_RESCHEDULE;
} // handle_mouse_interrupt

/////////////////////////////////////////////////////////////////////////
// file operations

/*
 * mouse_open:
 */
static int mouse_open(dev_ident ident, dev_cookie *cookie)
{
	*cookie = NULL;
	return 0;
} // mouse_open

/*
 * mouse_close:
 */
static int mouse_close(dev_cookie cookie)
{
	return 0;
} // mouse_close

/*
 * mouse_freecookie:
 */
static int mouse_freecookie(dev_cookie cookie)
{
	return 0;
} // mouse_freecookie

/*
 * mouse_seek:
 */
static int mouse_seek(dev_cookie cookie, off_t pos, seek_type st)
{
	return ERR_NOT_ALLOWED;
} // mouse_seek

/*
 * mouse_read:
 * Gets a mouse data packet.
 * Parameters:
 * dev_cookie, ignored
 * void*, pointer to a buffer that accepts the data
 * off_t, ignored
 * ssize_t, buffer size, must be at least the size of the data packet
 */
static ssize_t mouse_read(dev_cookie cookie, void* buf, off_t pos,
   ssize_t len)
{
   // inform interrupt handler that data is being waited for
   in_read = true;

   // wait until there is data to read
	if(sem_acquire_etc(mouse_sem, 1, SEM_FLAG_INTERRUPTABLE, 0, NULL) ==
	   ERR_INTERRUPTED) {
		return 0;
	} // if

	// verify user's buffer is of the right size
	if(len < PACKET_SIZE)
		return 0;

	// copy data to user's buffer
	((char*)buf)[0] = md_read.status;
	((char*)buf)[1] = md_read.delta_x;
	((char*)buf)[2] = md_read.delta_y;

	return PACKET_SIZE;
} // mouse_read

/*
 * mouse_write:
 */
static ssize_t mouse_write(dev_cookie cookie, const void *buf, off_t pos,
   ssize_t len)
{
	return ERR_VFS_READONLY_FS;
} // mouse_write

/*
 * mouse_ioctl:
 */
static int mouse_ioctl(dev_cookie cookie, int op, void *buf, size_t len)
{
	return ERR_INVALID_ARGS;
} // mouse_ioctl

/*
 * function structure used for file-op registration
 */
struct dev_calls ps2_mouse_hooks = {
	&mouse_open,
	&mouse_close,
	&mouse_freecookie,
	&mouse_seek,
	&mouse_ioctl,
	&mouse_read,
	&mouse_write,
	/* cannot page from mouse */
	NULL,
	NULL,
	NULL
}; // ps2_mouse_hooks

/////////////////////////////////////////////////////////////////////////
// initialization

/*
 * wait_write_ctrl:
 * Wait until the control port is ready to be written. This requires that
 * the "Input buffer full" and "Output buffer full" bits will both be set
 * to 0.
 */
static void wait_write_ctrl()
{
	while(isa->read_io_8(PS2_PORT_CTRL) & 0x3);
} // wait_for_ctrl_output

/*
 * wait_write_data:
 * Wait until the data port is ready to be written. This requires that
 * the "Input buffer full" bit will be set to 0.
 */
static void wait_write_data()
{
	while(isa->read_io_8(PS2_PORT_CTRL) & 0x2);
} // wait_write_data

/*
 * wait_read_data:
 * Wait until the data port can be read from. This requires that the
 * "Output buffer full" bit will be set to 1.
 */
static void wait_read_data()
{
	while((isa->read_io_8(PS2_PORT_CTRL) & 0x1) == 0);
} // wait_read_data

/*
 * write_command_byte:
 * Writes a command byte to the data port of the PS/2 controller.
 * Parameters:
 * unsigned char, byte to write
 */
static void write_command_byte(unsigned char b)
{
	wait_write_ctrl();
	isa->write_io_8(PS2_PORT_CTRL, PS2_CTRL_WRITE_CMD);
	wait_write_data();
	isa->write_io_8(PS2_PORT_DATA, b);
} // write_command_byte

/*
 * write_aux_byte:
 * Writes a byte to the mouse device. Uses the control port to indicate
 * that the byte is sent to the auxiliary device (mouse), instead of the
 * keyboard.
 * Parameters:
 * unsigned char, byte to write
 */
static void write_aux_byte(unsigned char b)
{
   wait_write_ctrl();
	isa->write_io_8(PS2_PORT_CTRL, PS2_CTRL_WRITE_AUX);
	wait_write_data();
	isa->write_io_8(PS2_PORT_DATA, b);
} // write_aux_byte

/*
 * read_data_byte:
 * Reads a single byte from the data port.
 * Return value:
 * unsigned char, byte read
 */
static unsigned char read_data_byte()
{
   wait_read_data();
	return isa->read_io_8(PS2_PORT_DATA);
} // read_data_byte

int dev_bootstrap(void);

/*
 * mouse_dev_init:
 * Called by the kernel to setup the device. Initializes the driver.
 * Parameters:
 * kernel_args*, ignored
 * Return value:
 * int, 0 if successful, negative error value otherwise
 */
int dev_bootstrap(void)
{
   dprintf("Initializing PS/2 mouse\n");

   if(module_get(ISA_MODULE_NAME, 0, (void **)&isa) < 0) {
	   dprintf("ps2mouse dev_bootstrap: no isa bus found..\n");
	   return -1;
   }

   // init device driver
	memset(&md_int, 0, sizeof(mouse_data));

	// empty Output buffer by reading from it
	if ( isa->read_io_8(PS2_PORT_CTRL) & 0x1 )
		isa->read_io_8(PS2_PORT_DATA);

	// XXX
	// There is a small race condition right here.
	// Does writing a control byte really require the Output buffer to be empty?

	// enable auxilary device, IRQs and PS/2 mouse
   write_command_byte(PS2_CMD_DEV_INIT);
	write_aux_byte(PS2_CMD_ENABLE_MOUSE);

	// controller should send ACK if mouse was detected
	if(read_data_byte() != PS2_RES_ACK) {
	   dprintf("No PS/2 mouse found\n");
	   module_put(ISA_MODULE_NAME);
		return -1;
	} // if

	dprintf("A PS/2 mouse has been successfully detected\n");

	// create the mouse semaphore, used for synchronization between
	// the interrupt handler and the read() operation
	mouse_sem = sem_create(0, "ps2_mouse_sem");
	if(mouse_sem < 0)
	   panic("failed to create PS/2 mouse semaphore!\n");

	// register interrupt handler
	int_set_io_interrupt_handler(INT_PS2_MOUSE,
	   &handle_mouse_interrupt, NULL, "ps2mouse");

	// register device file-system like operations
	devfs_publish_device("ps2mouse", NULL, &ps2_mouse_hooks);

	return 0;
} // mouse_dev_init
