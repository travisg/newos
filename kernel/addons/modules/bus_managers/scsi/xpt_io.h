/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __XPT_IO_H__
#define __XPT_IO_H__

void xpt_scsi_io( CCB_HEADER *ccb );
void xpt_abort( CCB_HEADER *ccb );
void xpt_term_io( CCB_HEADER *ccb );

bool xpt_check_exec_service( xpt_bus_info *bus );

void xpt_done_io( CCB_HEADER *ccb );

#endif
