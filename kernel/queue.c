/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/queue.h>


typedef struct queue_element {
	void *next;
} queue_element;

typedef struct queue_typed {
	queue_element *head;
	queue_element *tail;
	int count;
} queue_typed;

int queue_init(queue *q)
{
	q->head = q->tail = NULL;
	q->count = 0;
	return 0;
}

int queue_remove_item(queue *_q, void *e)
{
	queue_typed *q = (queue_typed *)_q;
	queue_element *elem = (queue_element *)e;
	queue_element *temp, *last = NULL;

	temp = (queue_element *)q->head;
	while(temp) {
		if(temp == elem) {
			if(last) {
				last->next = temp->next;
			} else {
				q->head = temp->next;
			}
			if(q->tail == temp)
				q->tail = last;
			q->count--;
			return 0;
		}
		last = temp;
		temp = temp->next;
	}

	return -1;
}

int queue_enqueue(queue *_q, void *e)
{
	queue_typed *q = (queue_typed *)_q;
	queue_element *elem = (queue_element *)e;

	if(q->tail == NULL) {
		q->tail = elem;
		q->head = elem;
	} else {
		q->tail->next = elem;
		q->tail = elem;
	}
	elem->next = NULL;
	q->count++;	
	return 0;
}

void *queue_dequeue(queue *_q)
{
	queue_typed *q = (queue_typed *)_q;
	queue_element *elem;

	elem = q->head;
	if(q->head != NULL)
		q->head = q->head->next;
	if(q->tail == elem)
		q->tail = NULL;
	
	if(elem != NULL)
		q->count--;

	return elem;
}

void *queue_peek(queue *q)
{
	return q->head;
}
