/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __ATA_H__
#define __ATA_H__

bool check_rw_error( ide_device_info *device, ide_qrequest *qrequest );
bool check_output( ide_device_info *device, bool drdy_required,
                   int error_mask, bool is_write );

bool prep_ata( ide_device_info *device );
void enable_CQ( ide_device_info *device, bool enable );
void ata_send_rw( ide_device_info *device, ide_qrequest *qrequest,
                  uint64 pos, size_t length, bool write );

void ata_dpc_DMA( ide_qrequest *qrequest );
void ata_dpc_PIO( ide_qrequest *qrequest );

void ata_exec_io( ide_device_info *device, ide_qrequest *qrequest );

#endif
