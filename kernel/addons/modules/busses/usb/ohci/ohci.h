/*
** Copyright 2003, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef __BUS_OHCI_H
#define __BUS_OHCI_H

#include <newos/types.h>

// USB Open Host Controller Interface
#define OHCI_BASE_CLASS     0x0c
#define OHCI_SUB_CLASS      0x03
#define OHCI_INTERFACE      0x10

// register bits
#define CONTROL_CBSR_MASK        0x00000003
#define CONTROL_CBSR_1TO1        0x00000000
#define CONTROL_CBSR_2TO1        0x00000001
#define CONTROL_CBSR_3TO1        0x00000002
#define CONTROL_CBSR_4TO1        0x00000003

#define CONTROL_PLE              0x00000004  // Periodic List Enable
#define CONTROL_IE               0x00000008  // Isochronous Enable
#define CONTROL_CLE              0x00000010  // Control List Enable
#define CONTROL_BLE              0x00000020  // Bulk List Enable
#define CONTROL_HCFS_MASK        0x000000C0  // Host Controller Function State
#define CONTROL_HCFS_RESET       0x00000000
#define CONTROL_HCFS_RESUME      0x00000040
#define CONTROL_HCFS_OPERATIONAL 0x00000080
#define CONTROL_HCFS_SUSPEND     0x000000C0
#define CONTROL_IR               0x00000100  // Interrupt Routing (SMI enable)
#define CONTROL_RWC              0x00000200  // Remote Wakeup Connected
#define CONTROL_RWE              0x00000400  // Remote Wakeup Enable

#define COMMAND_HCR              0x00000001  // Host Controller Reset
#define COMMAND_CLF              0x00000002  // Control List Filled
#define COMMAND_BLF              0x00000004  // Bulk List Filled
#define COMMAND_OCR              0x00000008  // Ownership Change Request

#define INT_SO                   0x00000001  // Scheduling Overrun
#define INT_WDH                  0x00000002  // Writeback Done Head
#define INT_SOF                  0x00000004  // Start of Frame
#define INT_RD                   0x00000008  // Resume Detected
#define INT_UE                   0x00000010  // Unrecoverable Error
#define INT_FNO                  0x00000020  // Frame Number Overflow
#define INT_RHSC                 0x00000040  // Root Hub Status Change
#define INT_OC                   0x40000000  // Ownership Change
#define INT_MIE                  0x80000000  // Master Interrupt Enable

// ohci operational registers
typedef struct ohci_regs {
	// control and status partition (p 109)
	vuint32 revision;
	vuint32 control;
	vuint32 command_status;
	vuint32 interrupt_status;
	vuint32 interrupt_enable;
	vuint32 interrupt_disable;

	// memory pointer partition (p 117)
	vuint32 HCCA;
	vuint32 period_current_ed;
	vuint32 control_head_ed;
	vuint32 control_current_ed;
	vuint32 bulk_head_ed;
	vuint32 bulk_current_ed;
	vuint32 done_head_ed;

	// frame counter partition (p 120)
	vuint32 frame_interval;
	vuint32 frame_remaining;
	vuint32 frame_number;
	vuint32 periodic_start;
	vuint32 ls_threshold;

	// root hub partition (p 123)
	vuint32 rh_descriptor_a;
	vuint32 rh_descriptor_b;
	vuint32 rh_status;
	vuint32 rh_port_status[15]; // max number of ports
} ohci_regs;

// ohci communications area (p 34)
typedef struct ohci_hcca
{
	vuint32 interrupt_table[32];
	vuint16 frame_number;
	vuint16 pad;
	vuint32 done_head;
	vuint8  reserved[120];
} ohci_hcca;

#define OHCI_HCCA_SIZE 0x4000

// ohci endpoint descriptor, page 16
typedef struct ohci_ed {
	// hardware portion
	vuint32 flags;
	vuint32 tail;
	vuint32 head;
	vuint32 next;

	// software portion
	addr_t phys_addr; // physical address of this endpoint
	struct ohci *oi; // remember what hc it belongs to

	struct ohci_queue *queue; // what queue it's present in
	struct ohci_ed *prev_ed;  // prev & next pointers in queue it's in
	struct ohci_ed *next_ed;

	struct ohci_td *tail_td; // last td in the list, virtual address

	usb_endpoint_descriptor *usb_ed;

	uint32 pad[1]; // pads it out to a multiple of 32
} ohci_ed;

// head field flags
#define OHCI_ED_HEAD_HALTED 	0x1
#define OHCI_ED_HEAD_CARRY		0x2

// ED flags
#define OHCI_ED_FLAGS_DIR_IN  	0x1000	// TDs are all in
#define OHCI_ED_FLAGS_DIR_OUT 	0x0800	// TDs are all out
#define OHCI_ED_FLAGS_DIR_MASK 	0x1800
#define OHCI_ED_FLAGS_DIR_TD	0x1800	// TD sets dir
#define OHCI_ED_FLAGS_SPEED 	0x2000	// low speed == 1, high speed == 0
#define OHCI_ED_FLAGS_SKIP		0x4000	// skip this ED
#define OHCI_ED_FLAGS_ISO		0x8000	// isochronous

// ohci transfer descriptor, page 20
typedef struct ohci_td {
	// hardware portion
	vuint32 flags;
	vuint32 curr_buf_ptr;
	vuint32 next_td;
	vuint32 buf_end;

	// software portion
	struct ohci_td *next;
	struct ohci_td *transfer_head; // points to the first ohci_td in this transfer
	struct ohci_td *transfer_next; // next one in the transfer
	struct ohci_ed *ed; // current endpoint descriptor this is part of

	addr_t phys_addr; // physical address of this descriptor
	bool last_in_transfer;
	usb_hc_transfer *usb_transfer;

	uint32 pad[1]; // pads it out to a multiple of 32
} ohci_td;

// TD flags
#define OHCI_TD_FLAGS_ROUNDING	(1 << 18)
#define OHCI_TD_FLAGS_DIR_SETUP	(0 << 19)
#define OHCI_TD_FLAGS_DIR_OUT	(1 << 19)
#define OHCI_TD_FLAGS_DIR_IN	(2 << 19)
#define OHCI_TD_FLAGS_DIR_MASK	(3 << 19)
#define OHCI_TD_FLAGS_IRQ_MASK	(7 << 21)
#define OHCI_TD_FLAGS_IRQ_1		(1 << 21)
#define OHCI_TD_FLAGS_IRQ_NONE	(7 << 21)
#define OHCI_TD_FLAGS_TOGGLE_0	(2 << 24)
#define OHCI_TD_FLAGS_TOGGLE_1	(3 << 24)
#define OHCI_TD_FLAGS_ERRCOUNT	(3 << 26)
#define OHCI_TD_FLAGS_CC_NOT	(14 << 28)
#define OHCI_TD_FLAGS_CC_MASK	(15 << 28)
#define OHCI_TD_FLAGS_CC(x) 	(((x) & OHCI_TD_FLAGS_CC_MASK) >> 28)

// ohci condition codes
typedef enum {
	OHCI_CC_NoError,
	OHCI_CC_CRC,
	OHCI_CC_BitStuffing,
	OHCI_CC_DataToggleMismatch,
	OHCI_CC_Stall,
	OHCI_CC_DeviceNotResponding,
	OHCI_CC_PIDCheckFailure,
	OHCI_CC_UnexpectedPID,
	OHCI_CC_DataOverrun,
	OHCI_CC_DataUnderrun,
	OHCI_CC_Reserved1,
	OHCI_CC_Reserved2,
	OHCI_CC_BufferOverrun,
	OHCI_CC_BufferUnderrun,
	OHCI_CC_NotAccessed,
	OHCI_CC_NotAccessed2
} ohci_cc;

// a queue of endpoints
typedef struct ohci_queue {
	ohci_ed *head;
	vuint32 *head_phys;
} ohci_queue;

// our data structure to represent an ohci hcf
typedef struct ohci {
	struct ohci *next;

	// data about the hcf
	int irq;
	region_id reg_region;
	ohci_regs *regs;
	region_id hcca_region;
	ohci_hcca *hcca;
	addr_t hcca_phys;

	// queues of endpoints
	ohci_queue control_queue;
	ohci_queue bulk_queue;
	ohci_queue interrupt_queue;
	mutex hc_list_lock; 			// lock around any endpoint/transfer descriptor list manipulation

	// a free pool of transfer descriptors
	ohci_td *td_freelist;
	mutex td_freelist_lock;

	// root hub stuff
	int rh_ports;

	// done list processing
	thread_id done_list_processor;
	sem_id done_list_sem;
} ohci;

#endif
