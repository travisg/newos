/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/heap.h>
#include <kernel/khash.h>
#include <kernel/debug.h>
#include <sys/errors.h>
#include <libc/string.h>

#define malloc kmalloc
#define free kfree

struct hash_table {
	struct hash_elem **table;
	int next_ptr_offset;
	int table_size;
	int num_elems;
	int flags;
	int (*compare_func)(void *e, void *key);
	unsigned int (*hash_func)(void *e, void *key, int range);
};	

// XXX gross hack
#define NEXT_ADDR(t, e) ((void *)(((unsigned long)(e)) + (t)->next_ptr_offset))
#define NEXT(t, e) ((void *)(*(unsigned long *)NEXT_ADDR(t, e)))
#define PUT_IN_NEXT(t, e, val) (*(unsigned long *)NEXT_ADDR(t, e) = (long)(val))

void *hash_init(int table_size, int next_ptr_offset,
	int compare_func(void *e, void *key),
	unsigned int hash_func(void *e, void *key, int range))
{
	struct hash_table *t;
	int i;
	
	t = (struct hash_table *)malloc(sizeof(struct hash_table));
	if(t == NULL) {
		return NULL;
	}

	t->table = (struct hash_elem **)malloc(sizeof(void *) * table_size);
	for(i = 0; i<table_size; i++)
		t->table[i] = NULL;
	t->table_size = table_size;
	t->next_ptr_offset = next_ptr_offset;
	t->flags = 0;
	t->num_elems = 0;
	t->compare_func = compare_func;
	t->hash_func = hash_func;

//	dprintf("hash_init: created table 0x%x, next_ptr_offset %d, compare_func 0x%x, hash_func 0x%x\n",
//		t, next_ptr_offset, compare_func, hash_func);

	return t;
}

int hash_uninit(void *_hash_table)
{
	struct hash_table *t = _hash_table;
	
#if 0
	if(t->num_elems > 0) {
		return -1;
	}
#endif	
	
	free(t->table);
	free(t);
	
	return 0;	
}

int hash_insert(void *_hash_table, void *e)
{
	struct hash_table *t = _hash_table;
	unsigned int hash;

//	dprintf("hash_insert: table 0x%x, element 0x%x\n", t, e);

	hash = t->hash_func(e, NULL, t->table_size);
	PUT_IN_NEXT(t, e, t->table[hash]);
	t->table[hash] = e;
	t->num_elems++;

	return 0;
}

int hash_remove(void *_hash_table, void *e)
{
	struct hash_table *t = _hash_table;
	void *i, *last_i;
	int hash;

	hash = t->hash_func(e, NULL, t->table_size);
	last_i = NULL;
	for(i = t->table[hash]; i != NULL; last_i = i, i = NEXT(t, i)) {
		if(i == e) {
			if(last_i != NULL)
				PUT_IN_NEXT(t, last_i, NEXT(t, i));
			else
				t->table[hash] = NEXT(t, i);
			t->num_elems--;
			return NO_ERROR;
		}
	}

	return ERR_GENERAL;
}

void *hash_find(void *_hash_table, void *e)
{
	struct hash_table *t = _hash_table;
	void *i;
	int hash;

	hash = t->hash_func(e, NULL, t->table_size);
	for(i = t->table[hash]; i != NULL; i = NEXT(t, i)) {
		if(i == e) {
			return i;
		}
	}

	return NULL;
}

void *hash_lookup(void *_hash_table, void *key)
{
	struct hash_table *t = _hash_table;
	void *i;
	int hash;

	if(t->compare_func == NULL)
		return NULL;

	hash = t->hash_func(NULL, key, t->table_size);
	for(i = t->table[hash]; i != NULL; i = NEXT(t, i)) {
		if(t->compare_func(i, key) == 0) {
			return i;
		}
	}

	return NULL;
}

struct hash_iterator *hash_open(void *_hash_table, struct hash_iterator *i)
{
	struct hash_table *t = _hash_table;

	if(i == NULL) {
		i = (struct hash_iterator *)malloc(sizeof(struct hash_iterator));
		if(i == NULL)
			return NULL;
	}

	hash_rewind(t, i);

	return i;
}

void hash_close(void *_hash_table, struct hash_iterator *i, bool free_iterator)
{
	if(free_iterator)
		free(i);
}

void hash_rewind(void *_hash_table, struct hash_iterator *i)
{
	struct hash_table *t = _hash_table;
	int index;

	for(index = 0; index < t->table_size; index++) {
		if(t->table[index] != NULL) {
			i->bucket = index;
			i->ptr = t->table[index];
			return;
		}
	}

	i->bucket = t->table_size;
	i->ptr = NULL;
}

void *hash_next(void *_hash_table, struct hash_iterator *i)
{
	struct hash_table *t = _hash_table;
	void *e;

	e = NULL;
findnext:
	if(i->ptr != NULL) {
		e = i->ptr;
		if(NEXT(t, i->ptr) != NULL) {
			i->ptr = NEXT(t, i->ptr);
			return e;
		}
	}

	if(i->bucket >= t->table_size)
		return NULL;
	for(i->bucket++; i->bucket < t->table_size; i->bucket++) {
		if(t->table[i->bucket] != NULL) {
			i->ptr = t->table[i->bucket];
			if(e != NULL) 
				return e;
			goto findnext;
		}
	}
	i->ptr = NULL;

	return e;
}

