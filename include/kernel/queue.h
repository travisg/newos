/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_QUEUE_H
#define _KERNEL_QUEUE_H

#include <kernel/kernel.h>

typedef struct queue {
	void *head;
	void *tail;
	int count;
} queue;

int queue_init(queue *q);
int queue_remove_item(queue *q, void *e);
int queue_enqueue(queue *q, void *e);
void *queue_dequeue(queue *q);
void *queue_peek(queue *q);

#endif

