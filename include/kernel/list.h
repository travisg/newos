/*
** Copyright 2001-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef __KERNEL_LIST_H
#define __KERNEL_LIST_H

/* A Linux-inspired circular doubly linked list */

struct list_node {
	struct list_node *prev;
	struct list_node *next;
};

static inline void list_initialize(struct list_node *list)
{
	list->prev = list->next = list;
}

static inline void list_clear_node(struct list_node *item)
{
	item->prev = item->next = 0;
}

static inline void list_add_head(struct list_node *list, struct list_node *item)
{
	item->next = list->next;
	item->prev = list;
	list->next->prev = item;
	list->next = item;
}

static inline void list_add_tail(struct list_node *list, struct list_node *item)
{
	item->prev = list->prev;
	item->next = list;
	list->prev->next = item;
	list->prev = item;
}

static inline void list_delete(struct list_node *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
	item->prev = item->next = NULL;
}

static inline struct list_node* list_remove_head(struct list_node *list)
{
	if(list->next != list) {
		struct list_node *item = list->next;
		list_delete(item);
		return item;
	} else {
		return NULL;
	}
}

static inline struct list_node* list_remove_tail(struct list_node *list)
{
	if(list->prev != list) {
		struct list_node *item = list->prev;
		list_delete(item);
		return item;
	} else {
		return NULL;
	}
}

static inline struct list_node* list_peek_head(struct list_node *list)
{
	if(list->next != list) {
		return list->next;
	} else {
		return NULL;
	}	
}

static inline struct list_node* list_peek_tail(struct list_node *list)
{
	if(list->prev != list) {
		return list->prev;
	} else {
		return NULL;
	}	
}

static inline struct list_node* list_next(struct list_node *list, struct list_node *item)
{
	if(item->next != list)
		return item->next;
	else if(item->next->next != list)
		return item->next->next;
	else
		return NULL;
}

static inline bool list_is_empty(struct list_node *list)
{
	return (list->next == list) ? true : false;
}

#define container_of(ptr, type, member) \
	((type *)((addr_t)(ptr) - ((addr_t)&(((type *)0)->member))))

#endif
