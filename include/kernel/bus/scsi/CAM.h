/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


// This interface isn't fully compliant to the official 
// CAM specification; especially many options like 
// passing virtual/physical address or pointers/array
// are fixed. I added a comment to all changes.

// There is no target mode support, let's hope noone
// ever needs it.

#ifndef __CAM_H__
#define __CAM_H__

#include <newos/types.h>
#include <kernel/sem.h>

/* Defines for the XPT function codes, according to CAM spec. */

/* Common function commands, 0x00 - 0x0F */
#define XPT_NOOP		0x00	/* Execute Nothing */
#define XPT_SCSI_IO		0x01	/* Execute the requested SCSI IO */
#define XPT_GDEV_TYPE	0x02	/* Get the device type information */
#define XPT_PATH_INQ	0x03	/* Path Inquiry */
#define XPT_REL_SIMQ	0x04	/* Release the SIM queue that is frozen */
#define XPT_SASYNC_CB	0x05	/* Set Async callback parameters */
#define XPT_SDEV_TYPE	0x06	/* Set the device type information */
#define XPT_SCAN_BUS	0x07	/* Scan the Scsi Bus */

/* XPT SCSI control functions, 0x10 - 0x1F */
#define XPT_ABORT		0x10	/* Abort the selected CCB */
#define XPT_RESET_BUS	0x11	/* Reset the SCSI bus */
#define XPT_RESET_DEV	0x12	/* Reset the SCSI device, BDR */
#define XPT_TERM_IO		0x13	/* Terminate the I/O process */
#define XPT_SCAN_LUN	0x14	/* Scan Logical Unit */

/* HBA engine commands, 0x20 - 0x2F */
#define XPT_ENG_INQ		0x20	/* HBA engine inquiry */
#define XPT_ENG_EXEC	0x21	/* HBA execute engine request */

/* Target mode commands, 0x30 - 0x3F */
#define XPT_EN_LUN		0x30	/* Enable LUN, Target mode support */
#define XPT_TARGET_IO	0x31	/* Execute the target IO request */
#define XPT_ACCEPT_TARG 0x32	/* Accept Host Target Mode CDB */
#define XPT_CONT_TARG	0x33	/* Cont. Host Target I/O Connection */
#define XPT_IMMED_NOTIFY 0x34	/* Notify Host Target driver of event*/
#define XPT_NOTIFY_ACK	0x35	/* Acknowledgement of event */

#define XPT_FUNC		0x7F	/* TEMPLATE */
#define XPT_VUNIQUE		0x80	/* All the rest are vendor unique commands */

/* ---------------------------------------------------------------------- */

/* General allocation length defines for the CCB structures. */

// TK: changed name and size
#define SCSI_MAX_CDB_SIZE	16	/* Space for the CDB bytes/pointer */
// TK: max size of sense data
#define SCSI_MAX_SENSE_SIZE	64

#define VUHBA		14	/* Vendor Unique HBA length */
#define SIM_ID		16	/* ASCII string len for SIM ID */
#define HBA_ID		16	/* ASCII string len for HBA ID */
#define SIM_PRIV	50	/* Length of SIM private data area */

/* Structure definitions for the CAM control blocks, CCB's for the subsystem. */

/* Common CCB header definition. */
// TK: all other, action specific structures can be modified by XPT/SIM,
// thus you can only reuse this header between two requests;
// don't change cam_path_id, cam_target_id or cam_target_len as 
// CCBs are allocated device-specifically
typedef struct ccb_header
{
	uint32          phys_addr;      /* physical address of this CCB */
	uint16			cam_ccb_len;	/* Length of the entire CCB */
	uchar			cam_func_code;	/* XPT function code */
	uchar			cam_status;		/* Returned CAM subsystem status */
	uchar			cam_hrsvd0;		/* Reserved field, for alignment */ 
	uchar			cam_path_id;	/* Path ID for the request */
	uchar			cam_target_id;	/* Target device ID */
	uchar			cam_target_lun;	/* Target LUN number */
	uint32			cam_flags;		/* Flags for operation of the subsystem */
	
	// TK: released once after execution of queued CCB
	// initialised by alloc_ccb, can be replaced for xpt_action but
	// must be restored before returning to free_ccb
	sem_id			completion_sem;
	
	struct xpt_bus_info *xpt_bus;
	//struct xpt_target_info *xpt_target;
	struct xpt_device_info *xpt_device;
	uchar 			xpt_state;
} CCB_HEADER;

/* Common SCSI functions. */

/* Union definition for the CDB space in the SCSI I/O request CCB */
// TK: unused 
typedef union cdb_un
{
	uchar	*cam_cdb_ptr;				/* Pointer to the CDB bytes to send */
	uchar	cam_cdb_bytes[ SCSI_MAX_CDB_SIZE ];	/* Area for the CDB to send */
} CDB_UN;

/* Get device type CCB */
typedef struct ccb_getdev
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	char		*cam_inq_data;			/* Ptr to the inquiry data space */
	uchar		cam_pd_type;			/* Periph device type from the TLUN */
} CCB_GETDEV;

/* Path inquiry CCB */
typedef struct ccb_pathinq
{
	CCB_HEADER	cam_ch;						/* Header information fields */
	uchar		cam_version_num;			/* Version number for the SIM/HBA */
	uchar		cam_hba_inquiry;			/* Mimic of INQ byte 7 for the HBA */
	uchar		cam_target_sprt;			/* Flags for target mode support */
	uchar		cam_hba_misc;				/* Misc HBA feature flags */
	uint16		cam_hba_eng_cnt;			/* HBA engine count */
	uchar		cam_vuhba_flags[ VUHBA ];	/* Vendor unique capabilities */
	uint32		cam_sim_priv;				/* Size of SIM private data area */
	uint32		cam_async_flags;			/* Event cap. for Async Callback */
	uchar		cam_hpath_id;				/* Highest path ID in the subsystem */
	uchar		cam_initiator_id;			/* ID of the HBA on the SCSI bus */
	uchar		cam_prsvd0;					/* Reserved field, for alignment */
	uchar		cam_prsvd1;					/* Reserved field, for alignment */
	char		cam_sim_vid[ SIM_ID ];		/* Vendor ID of the SIM */
	char		cam_hba_vid[ HBA_ID ];		/* Vendor ID of the HBA */
	uchar		*cam_osd_usage;				/* Ptr for the OSD specific area */
} CCB_PATHINQ;

/* Release SIM Queue CCB */
typedef struct ccb_relsim
{
	CCB_HEADER	cam_ch;						/* Header information fields */
	uint32		cam_frzn_count;				/* Frozen count of the queue */
} CCB_RELSIM;

/* SCSI I/O Request CCB */
typedef struct ccb_scsiio
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	
	// TK: not used
	//uchar		*cam_pdrv_ptr;			/* Ptr used by the Peripheral driver */
	//CCB_HEADER	*cam_next_ccb;			/* Ptr to the next CCB for action */
	//uchar		*cam_req_map;			/* Ptr for mapping info on the Req. */
	//void		(*cam_cbfcnp)(struct ccb_scsiio *);		
	//                                    /* Callback on completion function */
	
	// TK: new entries
	struct ccb_scsiio *xpt_next, *xpt_prev;
	
	// TK: replaced
	//uchar		*cam_data_ptr;			/* Pointer to the data buf/SG list */
	// TK: sg_list contains physical addresses if != NULL;
	//     cam_data should contain virtual address if data is virtually continuous;
	//     if there is no sg_list but cam_data, XPT automatically creates sg_list;
	//     only a few (emulated) commands depend on cam_data;
	//	   thus you should always provide sg_list
	uchar		*cam_data;
	struct sg_elem *cam_sg_list;		// SG list
	
	uint32		cam_dxfer_len;			/* Data xfer length */
	// TK: fixed sized array instead of pointer
	uchar		cam_sense_ptr[SCSI_MAX_SENSE_SIZE]; /* Pointer to the sense data buffer */
	// TK: unused
	uchar		cam_sense_len;			/* Num of bytes in the Autosense buf */
	uchar		cam_cdb_len;			/* Number of bytes for the CDB */
	uint16		cam_sglist_cnt;			/* Num of scatter gather list entries */
	// TK: enlarged for huge mediums; -1 means n/a
	int64		cam_sort;				/* Value used by SIM to sort on */
	uchar		cam_scsi_status;		/* Returned scsi device status */
	uchar		cam_sense_resid;		/* Autosense resid length: 2's comp */
	uchar		cam_osd_rsvd1[2];		/* OSD Reserved field, for alignment */
	int32		cam_resid;				/* Transfer residual length: 2's comp */
	
	// TK: must use command array
	uint8		cam_cdb[SCSI_MAX_CDB_SIZE]; /* Union for CDB bytes/pointer */
	uint32		cam_timeout;			/* Timeout value */
	
	// TK: unused (required for target mode only)
	//uchar		*cam_msg_ptr;			/* Pointer to the message buffer */
	//uint16		cam_msgb_len;			/* Num of bytes in the message buf */
	uint16		cam_vu_flags;			/* Vendor unique flags */
	uchar		cam_tag_action;			/* What to do for tag queuing */
	uchar		cam_iorsvd0[3];			/* Reserved field, for alignment */
	
	// TK: private XPT data
	bool		xpt_tmp_sg : 1;			// XPT created SG table for this request
	bool		xpt_ordered : 1;		// XPT blocks other requests
	uchar		cam_sim_priv[ SIM_PRIV ];	/* SIM private data area */
} CCB_SCSIIO;

/* Set Async Callback CCB */
typedef struct ccb_setasync
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	uint32		cam_async_flags;		/* Event enables for Callback resp */
	void		(*cam_async_func)();	/* Async Callback function address */
	uchar		*pdrv_buf;				/* Buffer set aside by the Per. drv */
	uchar		pdrv_buf_len;			/* The size of the buffer */
} CCB_SETASYNC;

/* Set device type CCB */
typedef struct ccb_setdev
{
	CCB_HEADER	 cam_ch;				/* Header information fields */
	uchar		cam_dev_type;			/* Val for the dev type field in EDT */
} CCB_SETDEV;

/* SCSI Control Functions. */

/* Abort XPT Request CCB */
typedef struct ccb_abort
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	CCB_HEADER	*cam_abort_ch;			/* Pointer to the CCB to abort */
} CCB_ABORT;

/* Reset SCSI Bus CCB */
typedef struct ccb_resetbus
{
	CCB_HEADER	cam_ch;					/* Header information fields */
} CCB_RESETBUS;

/* Reset SCSI Device CCB */
typedef struct ccb_resetdev
{
	CCB_HEADER	cam_ch;					/* Header information fields */
} CCB_RESETDEV;

/* Terminate I/O Process Request CCB */
typedef struct ccb_termio
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	CCB_HEADER	*cam_termio_ch;			/* Pointer to the CCB to terminate */
} CCB_TERMIO;

/* Target mode structures. */

/* Host Target Mode Version 1 Enable LUN CCB */
typedef struct ccb_en_lun
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	uint16		cam_grp6_len;			/* Group 6 VU CDB length */
	uint16		cam_grp7_len;			/* Group 7 VU CDB length */
	uchar		*cam_ccb_listptr;		/* Pointer to the target CCB list */
	uint16		cam_ccb_listcnt;		/* Count of Target CCBs in the list */
} CCB_EN_LUN;

/* Enable LUN CCB (HTM V2) */
typedef struct ccb_enable_lun
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	uint16		cam_grp6_length;		/* Group 6 Vendor Unique CDB Lengths */
	uint16		cam_grp7_length;		/* Group 7 Vendor Unique CDB Lengths */
	uchar		*cam_immed_notify_list;	/* Ptr to Immediate Notify CCB list */
	uint32		cam_immed_notify_cnt;	/* Number of Immediate Notify CCBs */
	uchar		*cam_accept_targ_list;	/* Ptr to Accept Target I/O CCB list */
	uint32		cam_accept_targ_cnt;	/* Number of Accept Target I/O CCBs */
	uchar		cam_sim_priv[ SIM_PRIV ]; /* SIM private data area */
} CCB_ENABLE_LUN;

/* Immediate Notify CCB */
typedef struct ccb_immed_notify
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	uchar		*cam_pdrv_ptr;			/* Ptr used by the Peripheral driver */
	void		(*cam_cbfnot)();		/* Callback on notification function */
	uchar		*cam_sense_ptr;			/* Pointer to the sense data buffer */
	uchar		cam_sense_len;			/* Num of bytes in the Autosense buf */
	uchar		cam_init_id;			/* ID of Initiator that selected */
	uint16		cam_seq_id;				/* Sequence Identifier */
	uchar		cam_msg_code;			/* Message Code */
	uchar		cam_msg_args[7];		/* Message Arguments */
} CCB_IMMED_NOTIFY;

/* Notify Acknowledge CCB */
typedef struct ccb_notify_ack
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	uint16		cam_seq_id;				/* Sequence Identifier */
	uchar		cam_event;				/* Event */
	uchar		cam_rsvd;
} CCB_NOTIFY_ACK;

/* Accept Target I/O CCB */
typedef struct ccb_accept_targ
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	uchar		*cam_pdrv_ptr;			/* Ptr used by the Peripheral driver */
	CCB_HEADER	*cam_next_ccb;			/* Ptr to the next CCB for action */
	uchar		*cam_req_map;			/* Ptr for mapping info on the Req. */
	void		(*cam_cbfcnot)();		/* Callback on completion function */
	uchar		*cam_data_ptr;			/* Pointer to the data buf/SG list */
	uint32		cam_dxfer_len;			/* Data xfer length */
	uchar		*cam_sense_ptr;			/* Pointer to the sense data buffer */
	uchar		cam_sense_len;			/* Num of bytes in the Autosense buf */
	uchar		cam_cdb_len;			/* Number of bytes for the CDB */
	uint16		cam_sglist_cnt;			/* Num of scatter gather list entries */
	uint32  	cam_sort;				/* Value used by SIM to sort on */
	uchar		cam_scsi_status;		/* Returned scsi device status */
	uchar		cam_sense_resid;		/* Autosense resid length: 2's comp */
	uchar		cam_osd_rsvd1[2];		/* OSD Reserved field, for alignment */
	int32		cam_resid;				/* Transfer residual length: 2's comp */
	CDB_UN		cam_cdb_io;				/* Union for CDB bytes/pointer */
	uint32		cam_timeout;			/* Timeout value */
	uchar		*cam_msg_ptr;			/* Pointer to the message buffer */
	uint16		cam_msgb_len;			/* Num of bytes in the message buf */
	uint16		cam_vu_flags;			/* Vendor unique flags */
	uchar		cam_tag_action;			/* What to do for tag queuing */
	uchar		cam_tag_id;				/* Tag ID */
	uchar		cam_initiator_id;		/* Initiator ID */
	uchar		cam_iorsvd0[1];			/* Reserved field, for alignment */
	uchar		cam_sim_priv[ SIM_PRIV ]; /* SIM private data area */
} CCB_ACCEPT_TARG;

/* Continue Target I/O CCB */
typedef CCB_ACCEPT_TARG CCB_CONT_TARG;

/* HBA engine structures. */

typedef struct ccb_eng_inq
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	uint16		cam_eng_num;			/* The number for this inquiry */
	uchar		cam_eng_type;			/* Returned engine type */
	uchar		cam_eng_algo;			/* Returned algorithm type */
	uint32		cam_eng_memory;			/* Returned engine memory size */
} CCB_ENG_INQ;

typedef struct ccb_eng_exec	/* NOTE: must match SCSIIO size */
{
	CCB_HEADER	cam_ch;					/* Header information fields */
	uchar		*cam_pdrv_ptr;			/* Ptr used by the Peripheral driver */
	uint32		cam_engrsvd0;			/* Reserved field, for alignment */
	uchar		*cam_req_map;			/* Ptr for mapping info on the Req. */
	void		(*cam_cbfcnp)();		/* Callback on completion function */
	uchar		*cam_data_ptr;			/* Pointer to the data buf/SG list */
	uint32		cam_dxfer_len;			/* Data xfer length */
	uchar		*cam_engdata_ptr;		/* Pointer to the engine buffer data */
	uchar		cam_engrsvd1;			/* Reserved field, for alignment */
	uchar		cam_engrsvd2;			/* Reserved field, for alignment */
	uint16		cam_sglist_cnt;			/* Num of scatter gather list entries */
	uint32		cam_dmax_len;			/* Destination data maximum length */
	uint32		cam_dest_len;			/* Destination data length */
	int32		cam_src_resid;			/* Source residual length: 2's comp */
	uchar		cam_engrsvd3[12];		/* Reserved field, for alignment */
	uint32		cam_timeout;			/* Timeout value */
	uint32		cam_engrsvd4;			/* Reserved field, for alignment */
	uint16		cam_eng_num;			/* Engine number for this request */
	uint16		cam_vu_flags;			/* Vendor unique flags */
	uchar		cam_engrsvd5;			/* Reserved field, for alignment */
	uchar		cam_engrsvd6[3];		/* Reserved field, for alignment */
	uchar		cam_sim_priv[ SIM_PRIV ]; /* SIM private data area */
} CCB_ENG_EXEC;

/* The sim_module_info definition is used to define the entry points for
the SIMs contained in the SCSI CAM subsystem.	Each SIM file will
contain a declaration for it's entry.	The address for this entry will
be stored in the cam_conftbl[] array along will all the other SIM
entries. */

typedef struct cam_sim_cookie *cam_sim_cookie;

typedef struct cam_sim_interface {
	//int				(*init)();
	// TK: no return value any more
	void (*action)( cam_sim_cookie cookie, CCB_HEADER *ccb );
	// called when all connections to SIM are closed
	void (*unregistered)( cam_sim_cookie cookie );
} cam_sim_interface;


/* ---------------------------------------------------------------------- */

/* Defines for the CAM status field in the CCB header. */

#define	CAM_REQ_INPROG			0x00	/* CCB request is in progress */
#define CAM_REQ_CMP 			0x01	/* CCB request completed w/out error */
#define CAM_REQ_ABORTED			0x02	/* CCB request aborted by the host */
#define CAM_UA_ABORT			0x03	/* Unable to Abort CCB request */
#define CAM_REQ_CMP_ERR			0x04	/* CCB request completed with an err */
#define CAM_BUSY				0x05	/* CAM subsystem is busy */
#define CAM_REQ_INVALID			0x06	/* CCB request is invalid */
#define CAM_PATH_INVALID		0x07	/* Path ID supplied is invalid */
#define CAM_DEV_NOT_THERE		0x08	/* SCSI device not installed/there */
#define CAM_UA_TERMIO			0x09	/* Unable to Terminate I/O CCB req */
#define CAM_SEL_TIMEOUT			0x0A	/* Target selection timeout */
#define CAM_CMD_TIMEOUT			0x0B	/* Command timeout */
#define CAM_MSG_REJECT_REC		0x0D	/* Message reject received */
#define CAM_SCSI_BUS_RESET		0x0E	/* SCSI bus reset sent/received */
#define CAM_UNCOR_PARITY		0x0F	/* Uncorrectable parity err occurred */
#define CAM_AUTOSENSE_FAIL		0x10	/* Autosense: Request sense cmd fail */
#define CAM_NO_HBA				0x11	/* No HBA detected Error */
#define CAM_DATA_RUN_ERR		0x12	/* Data overrun/underrun error */
#define CAM_UNEXP_BUSFREE		0x13	/* Unexpected BUS free */
#define CAM_SEQUENCE_FAIL		0x14	/* Target bus phase sequence failure */
#define CAM_CCB_LEN_ERR			0x15	/* CCB length supplied is inadequate */
#define CAM_PROVIDE_FAIL		0x16	/* Unable to provide requ. capability */
#define CAM_BDR_SENT			0x17	/* A SCSI BDR msg was sent to target */
#define CAM_REQ_TERMIO			0x18	/* CCB request terminated by the host */
#define CAM_HBA_ERR				0x19	/* Unrecoverable host bus adaptor err*/
#define CAM_BUS_RESET_DENIED	0x1A	/* SCSI bus reset denied */

// TK: internal only! (XXX any better number for this?)
#define CAM_DEV_QUEUE_FULL		0x20	/* SIM device queue full */
#define CAM_BUS_QUEUE_FULL		0x21	/* SIM bus queue full */
#define CAM_RESUBMIT			0x22	/* resubmit request */

#define CAM_IDE					0x33	/* Initiator Detected Error Received */
#define CAM_RESRC_UNAVAIL		0x34	/* Resource unavailable */
#define CAM_UNACKED_EVENT		0x35	/* Unacknowledged event by host */
#define CAM_MESSAGE_RECV		0x36	/* Msg received in Host Target Mode */
#define CAM_INVALID_CDB			0x37	/* Invalid CDB recvd in HT Mode */
#define CAM_LUN_INVALID			0x38	/* LUN supplied is invalid */
#define CAM_TID_INVALID			0x39	/* Target ID supplied is invalid */
#define CAM_FUNC_NOTAVAIL		0x3A	/* The requ. func is not available */
#define CAM_NO_NEXUS			0x3B	/* Nexus is not established */
#define CAM_IID_INVALID			0x3C	/* The initiator ID is invalid */
#define CAM_CDB_RECVD			0x3D	/* The SCSI CDB has been received */
#define CAM_LUN_ALLREADY_ENAB	0x3E	/* LUN already enabled */
#define CAM_SCSI_BUSY			0x3F	/* SCSI bus busy */

// TK: not supported
#define CAM_SIM_QFRZN			0x40	/* The SIM queue is frozen w/this err */
#define CAM_AUTOSNS_VALID		0x80	/* Autosense data valid for target */

#define CAM_STATUS_MASK			0x3F	/* Mask bits for just the status # */

/* ---------------------------------------------------------------------- */

/* Defines for the CAM flags field in the CCB header. */

#define CAM_DIR_RESV			0x00000000	/* Data direction (00: reserved) */
#define CAM_DIR_IN				0x00000040	/* Data direction (01: DATA IN) */
#define CAM_DIR_OUT				0x00000080	/* Data direction (10: DATA OUT) */
#define CAM_DIR_NONE			0x000000C0	/* Data direction (11: no data) */
// TK: added helper mask
#define CAM_DIR_MASK			0x000000C0
// TK: probably not supported in future
#define CAM_DIS_AUTOSENSE		0x00000020	/* Disable autosense feature */
// TK: n/a, if sg_list != NULL, it is valid
//#define CAM_SCATTER_VALID		0x00000010	/* Scatter/gather list is valid */
// TK: n/a, no callback
//#define CAM_DIS_CALLBACK		0x00000008	/* Disable callback feature */
// TK: not supported
//#define CAM_CDB_LINKED			0x00000004	/* The CCB contains a linked CDB */
// TK: use cam_tag_action instead
//#define CAM_QUEUE_ENABLE		0x00000002	/* SIM queue actions are enabled */
// TK we always use the fixed sized array
//#define CAM_CDB_POINTER			0x00000001	/* The CDB field contains a pointer */

// TK: new flag - currently ignored
#define CAM_SYNC_EXEC			0x00000100	/* execute synchronously */

#define CAM_DIS_DISCONNECT 		0x00008000	/* Disable disconnect */
#define CAM_INITIATE_SYNC		0x00004000	/* Attempt Sync data xfer, and SDTR */
#define CAM_DIS_SYNC			0x00002000	/* Disable sync, go to async */
// TK: usually pointless, as XPT's own queueing doesn't support this feature
#define CAM_SIM_QHEAD			0x00001000	/* Place CCB at the head of SIM Q */
// TK: not supported
//#define CAM_SIM_QFREEZE			0x00000800	/* Return the SIM Q to frozen state */
// TK: not supported
//#define CAM_SIM_QFRZDIS			0x00000400	/* Disable the SIM Q frozen state */
#define CAM_ENG_SYNC			0x00000200	/* Flush resid bytes before cmplt */

#define CAM_ENG_SGLIST			0x00800000	/* The SG list is for the HBA engine */
// TK: always stored in array as part of ccb
//#define CAM_CDB_PHYS			0x00400000	/* CDB pointer is physical */
// TK: cam_data must be virtual
//#define CAM_DATA_PHYS			0x00200000	/* SG/Buffer data ptrs are physical */
// TK: n/a, as sense data is stored as part of ccb
//#define CAM_SNS_BUF_PHYS		0x00100000	/* Autosense data ptr is physical */
#define CAM_MSG_BUF_PHYS		0x00080000	/* Message buffer ptr is physical */
// TK: linked requests not supported
//#define CAM_NXT_CCB_PHYS		0x00040000	/* Next CCB pointer is physical */
// TK: call backs not supported
//#define CAM_CALLBCK_PHYS		0x00020000	/* Callback func ptr is physical */
// TK: sg_list entries must be physical
//#define CAM_SG_LIST_PHYS		0x00010000	/* SG list pointers physical */

/* Phase cognizant mode flags */
#define CAM_DATAB_VALID			0x80000000	/* Data buffer valid */
#define CAM_STATUS_VALID		0x40000000	/* Status buffer valid */
#define CAM_MSGB_VALID			0x20000000	/* Message buffer valid */
#define CAM_TGT_PHASE_MODE 		0x08000000	/* The SIM will run in phase mode */
#define CAM_TGT_CCB_AVAIL		0x04000000	/* Target CCB available */
#define CAM_DIS_AUTODISC		0x02000000	/* Disable autodisconnect */
#define CAM_DIS_AUTOSRP			0x01000000	/* Disable autosave/restore ptrs */

/* Host target mode flags */
#define CAM_SEND_STATUS			0x80000000	/* Send status after date phase */
#define CAM_DISCONNECT			0x40000000	/* Disc. mandatory after cdb recv */
#define CAM_TERM_IO				0x20000000	/* Terminate I/O Message supported */
#define CAM_TGT_PHASE_MODE		0x08000000	/* The SIM runs in phase mode */


/* ---------------------------------------------------------------------- */

/* Defines for the SIM/HBA queue actions.	These value are used in the
SCSI I/O CCB, for the queue action field. [These values should match the
defines from some other include file for the SCSI message phases.	We may
not need these definitions here. ] */

// TK: new value to make queuing flag unnecessary
#define CAM_NO_QTAG				0x00		/* Queuing unimportant */
#define CAM_SIMPLE_QTAG			0x20		/* Tag for a simple queue */
#define CAM_HEAD_QTAG			0x21		/* Tag for head of queue */
#define CAM_ORDERED_QTAG		0x22		/* Tag for ordered queue */

/* ---------------------------------------------------------------------- */

/* Defines for the timeout field in the SCSI I/O CCB.	At this time a value
of 0xF-F indicates a infinite timeout.	A value of 0x0-0 indicates that the
SIM's default timeout can take effect. */

#define CAM_TIME_DEFAULT		0x00000000	/* Use SIM default value */
#define CAM_TIME_INFINITY		0xFFFFFFFF	/* Infinite timeout for I/O */

/* ---------------------------------------------------------------------- */

/* Defines for the Path Inquiry CCB fields. */

#define	CAM_VERSION				0x25	/* Binary value for the current ver */

#define PI_MDP_ABLE				0x80	/* Supports MDP message */
#define PI_WIDE_32				0x40	/* Supports 32 bit wide SCSI */
#define PI_WIDE_16				0x20	/* Supports 16 bit wide SCSI */
#define PI_SDTR_ABLE			0x10	/* Supports SDTR message */
#define PI_LINKED_CDB			0x08	/* Supports linked CDBs */
#define PI_TAG_ABLE				0x02	/* Supports tag queue message */
#define PI_SOFT_RST				0x01	/* Supports soft reset */

#define PIT_PROCESSOR			0x80	/* Target mode processor mode */
#define PIT_PHASE				0x40	/* Target mode phase cog. mode */
#define PIT_DISCONNECT			0x20	/* Disconnects supported in target mode  */
#define PIT_TERM_IO				0x20	/* Terminate I/O message support in target mode */
#define PIT_GRP_8				0x20	/* Group 8 commands supported */
#define PIT_GRP_7				0x20	/* Group 7 commands supported */

#define PIM_SCANHILO			0x80	/* Bus scans from ID 7 to ID 0 */
#define PIM_NOREMOVE			0x40	/* Removable dev not included in scan */
#define PIM_NOINQUIRY			0x20	/* INQUIRY data not kept by XPT */

/* ---------------------------------------------------------------------- */

/* Defines for Asynchronous Callback CCB fields. */

// TK: called for each new device; thus target and lun are valid
// you'll never receive "lost" for a device that didn't exist before and vice versa
#define AC_FOUND_DEVICE			0x80	/* During a rescan new device found */
// TK: called for each lost device
#define AC_LOST_DEVICE			0x04	
#define AC_SIM_DEREGISTER		0x40	/* A loaded SIM has de-registered */
#define AC_SIM_REGISTER			0x20	/* A loaded SIM has registered */
#define AC_SENT_BDR				0x10	/* A BDR message was sent to target */
#define AC_SCSI_AEN				0x08	/* A SCSI AEN has been received */
#define AC_UNSOL_RESEL			0x02	/* A unsolicited reselection occurred */
#define AC_BUS_RESET			0x01	/* A SCSI bus RESET occurred */

/* ---------------------------------------------------------------------- */

/* Typedef for a scatter/gather list element. */

typedef struct sg_elem
{
	uchar		*cam_sg_address;		/* Scatter/Gather address */
	uint32		cam_sg_count;			/* Scatter/Gather count */
} SG_ELEM;

/* ---------------------------------------------------------------------- */

/* Defines for the HBA engine inquiry CCB fields. */

#define EIT_BUFFER				0x00	/* Engine type: Buffer memory */
#define EIT_LOSSLESS			0x01	/* Engine type: Lossless compression */
#define EIT_LOSSLY				0x02	/* Engine type: Lossly compression */
#define EIT_ENCRYPT				0x03	/* Engine type: Encryption */

#define EAD_VUNIQUE				0x00	/* Eng algorithm ID: vendor unique */
#define EAD_LZ1V1				0x00	/* Eng algorithm ID: LZ1 var. 1*/
#define EAD_LZ2V1				0x00	/* Eng algorithm ID: LZ2 var. 1*/
#define EAD_LZ2V2				0x00	/* Eng algorithm ID: LZ2 var. 2*/

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

/* Unix OSD defines and data structures. */

#define INQLEN	36		/* Inquiry string length to store. */

#define CAM_SUCCESS	0	/* For signaling general success */
#define CAM_FAILURE	1	/* For signaling general failure */

#define CAM_FALSE	0	/* General purpose flag value */
#define CAM_TRUE	1	/* General purpose flag value */

#define XPT_CCB_INVALID	-1	/* for signaling a bad CCB to free */

/* The typedef for the Async callback information.	This structure is used to
store the supplied info from the Set Async Callback CCB, in the EDT table
in a linked list structure. */

typedef struct async_info
{
	struct async_info	*cam_async_next;		/* pointer to the next structure */
	uint32				cam_event_enable;		/* Event enables for Callback resp */
	void				(*cam_async_func)();	/* Async Callback function address */
	uint32				cam_async_blen;			/* Length of "information" buffer */
	uchar				*cam_async_ptr;			/* Address for the "information */
} ASYNC_INFO;

/* The CAM EDT table contains the device information for all the
devices, SCSI ID and LUN, for all the SCSI busses in the system.	The
table contains a CAM_EDT_ENTRY structure for each device on the bus.
*/

typedef struct cam_edt_entry
{
	int32		cam_tlun_found;			/* Flag for the existence of the target/LUN */
	ASYNC_INFO	*cam_ainfo;				/* Async callback list info for this B/T/L */
	uint32		cam_owner_tag;			/* Tag for the peripheral driver's ownership */
	char		cam_inq_data[ INQLEN ];	/* storage for the inquiry data */
} CAM_EDT_ENTRY;


/* ============================================================================== */
/* ----------------------------- VENDOR UNIQUE DATA ----------------------------- */
/* ============================================================================== */


// inquiry host controller restrictions
#define XPT_INQUIRY_PARAMS	(XPT_VUNIQUE + 1)

typedef struct cam_sim_params {
	int dma_alignment;			// block alignment - this must 2^i-1 for some i
	size_t dma_boundary;		// (mask of bits that can change in one SG block) + 1
	bool dma_boundary_solid;	// true if boundary can be overcome by splitting blocks
	
	int max_sg_num;				// maximum number of SG blocks
	int max_blocks;				// maximum number of blocks
} CAM_SIM_PARAMS;

typedef struct cam_inquiry_params {
	CCB_HEADER cam_ch;
	CAM_SIM_PARAMS sim_params;	
} CAM_INQUIRY_PARAMS;


/* ---
	XPT interface used by SCSI drivers
--- */

//typedef struct cam_for_driver_module_info cam_for_driver_module_info;
typedef struct cam_periph_info *cam_periph_cookie;

typedef struct cam_periph_interface {
	void (*async)( cam_periph_cookie periph_cookie, 
		int path_id, int target_id, int lun, 
		int code, const void *data, int data_len );
} cam_periph_interface;

typedef struct xpt_periph_info *xpt_periph_cookie;
typedef struct xpt_bus_info *xpt_bus_cookie;
typedef struct xpt_device_info *xpt_device_cookie;


typedef struct cam_for_driver_interface {
	xpt_bus_cookie (*get_bus)( uchar path_id );
	int (*put_bus)( xpt_bus_cookie bus );
	
	// pass XPT_BUS_GLOBAL_ID as target and lun to get bus-global handle 
	xpt_device_cookie (*get_device)( xpt_bus_cookie bus, int target_id, int target_lun );
	int (*put_device)( xpt_device_cookie device );
	
	CCB_HEADER *(*ccb_alloc)( xpt_device_cookie device );
	void (*ccb_free)( CCB_HEADER *ccb );
	void (*action)( CCB_HEADER *ccbh );

	// during registration, you get an AC_FOUND_DEVICE for each existing device
	xpt_periph_cookie (*register_driver)( cam_periph_interface *interface,
		cam_periph_cookie periph_cookie );
	int (*unregister_driver)( xpt_periph_cookie cookie );
} cam_for_driver_interface;

#define CAM_FOR_DRIVER_MODULE_NAME "bus_managers/scsi/driver/v1"


/* ---
	XPT interface used by SCSI SIMs
--- */

//typedef struct xpt_bus_info *xpt_bus_cookie;

#define XPT_DONE_IN_IRQ 1

typedef struct xpt_dpc_info *xpt_dpc_cookie;

typedef struct cam_for_sim_interface {
	void (*done)( CCB_HEADER *ccb );
	
	// following functions return error on invalid arguments only

	int (*alloc_dpc)( xpt_dpc_cookie *dpc );
	int (*free_dpc)( xpt_dpc_cookie dpc );
	int (*schedule_dpc)( xpt_bus_cookie cookie, xpt_dpc_cookie dpc, /*int flags,*/
		void (*func)( void * ), void *arg );
		
	int (*block_bus)( xpt_bus_cookie bus );
	int (*unblock_bus)( xpt_bus_cookie bus );
	int (*block_device)( xpt_bus_cookie bus, int target, int lun );
	int (*unblock_device)( xpt_bus_cookie bus, int target, int lun );
	
	int (*cont_send_bus)( xpt_bus_cookie bus );
	int (*cont_send_device)( xpt_bus_cookie bus, int target_id, int lun );
	
	int (*call_async)( xpt_bus_cookie bus, int target_id, int lun, 
		int code, uchar *data, int data_len );
	int (*flush_async)( xpt_bus_cookie bus );

	// these functions can fail because of internal problems
	
	// returns path_id or error code
	int (*register_SIM)( cam_sim_interface *interface, cam_sim_cookie cookie,
		xpt_bus_cookie *bus );
	// if there are open connection, the SIM must still handle them
	// by refusing all requests, returning CAM_NO_HBA
	int (*unregister_SIM)( int path_id );
} cam_for_sim_interface;

#define CAM_FOR_SIM_MODULE_NAME "bus_managers/scsi/sim/v1"


/* General Union for Kernel Space allocation.	Contains all the possible CCB
structures.	This union should never be used for manipulating CCB's its only
use is for the allocation and deallocation of raw CCB space. */

typedef union ccb_size_union
{
	CCB_SCSIIO			csio;	/* Please keep this first, for debug/print */
	CCB_GETDEV			cgd;
	CCB_PATHINQ			cpi;
	CCB_RELSIM			crs;
	CCB_SETASYNC		csa;
	CCB_SETDEV			csd;
	CCB_ABORT			cab;
	CCB_RESETBUS		crb;
	CCB_RESETDEV		crd;
	CCB_TERMIO			ctio;
	CCB_EN_LUN			cel;
	CCB_ENABLE_LUN		cel2;
	CCB_IMMED_NOTIFY	cin;
	CCB_NOTIFY_ACK		cna;
	CCB_ACCEPT_TARG		cat;
	CCB_ENG_INQ			cei;
	CCB_ENG_EXEC		cee;
	//CCB_EXTENDED_PATHINQ	cdpi;
	CAM_INQUIRY_PARAMS	cep;
} CCB_SIZE_UNION;

// TK: extra stuff
#define XPT_XPT_PATH_ID 0xFF	/* path id of XPT itself (used by XPT_PATH_INQ) */
#define XPT_BUS_GLOBAL_ID 0xFF	/* target id and lun of entire bus (for get_device) */

#endif
