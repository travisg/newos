#ifndef _KHASH_H
#define _KHASH_H

void *hash_init(int table_size, int next_ptr_offset,
	int compare_func(void *a, void *key),
	int hash_func(void *a, void *key, int range));
int hash_uninit(void *_hash_table);
int hash_insert(void *_hash_table, void *_elem);
int hash_remove(void *_hash_table, void *_elem);
void *hash_lookup(void *_hash_table, void *key);
void *hash_open(void *_hash_table);
void hash_close(void *_hash_table, void *_iterator);
void *hash_next(void *_hash_table, void *_iterator);
void hash_rewind(void *_hash_table, void *_iterator);

/* function ptrs must look like this:
	// hash function should calculate hash on either e or key,
	// depending on which one is not NULL
int hash_func(void *e, void *key, int range);
	// compare function should compare the element with
	// the key, returning 0 if equal, other if not
int compare_func(void *e, void *key);
*/

#endif

