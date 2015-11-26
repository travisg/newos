/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __PIO_H__
#define __PIO_H__


void prep_PIO_transfer( ide_device_info *device, ide_qrequest *qrequest );
int read_PIO_block( ide_qrequest *qrequest, int length );
int write_PIO_block( ide_qrequest *qrequest, int length );


#endif
