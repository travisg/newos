/* 
** Copyright 2002, Thomas Kurschel. All rights reserved.
** Distributed under the terms of the NewOS License.
*/


#ifndef __PERIPH_INTERFACE_H__
#define __PERIPH_INTERFACE_H__

void xpt_done_nonio( CCB_HEADER *ccb );
void xpt_action( CCB_HEADER *ccb );

#endif
