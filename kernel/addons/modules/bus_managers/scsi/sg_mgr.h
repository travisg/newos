/*
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __SG_MGR_H__
#define __SG_MGR_H__

bool create_temp_sg( CCB_SCSIIO *ccb );
void cleanup_tmp_sg( CCB_SCSIIO *ccb );

int init_temp_sg( void );
void uninit_temp_sg( void );

#endif
