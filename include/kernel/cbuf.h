#ifndef _CBUF_H
#define _CBUF_H

#define CBUF_LEN 512

#define CBUF_FLAG_CHAIN_HEAD 1
#define CBUF_FLAG_CHAIN_TAIL 2

typedef struct cbuf {
	struct cbuf *next;
	int len;
	int total_len;
	void *data;
	int flags;
	char dat[CBUF_LEN - sizeof(struct cbuf *) - 2*sizeof(int) - sizeof(void *) - sizeof(int)];
} cbuf;

int cbuf_init();
cbuf *cbuf_get_chain(int len);
cbuf *cbuf_get_chain_noblock(int len);
void cbuf_free_chain_noblock(cbuf *buf);
void cbuf_free_chain(cbuf *buf);

int cbuf_get_len(cbuf *buf);
void *cbuf_get_ptr(cbuf *buf, int offset);
int cbuf_is_contig_region(cbuf *buf, int start, int end);

int cbuf_memcpy_to_chain(cbuf *chain, int offset, const void *_src, int len);
int cbuf_memcpy_from_chain(void *dest, cbuf *chain, int offset, int len);

cbuf *cbuf_merge_chains(cbuf *chain1, cbuf *chain2);

int cbuf_truncate_head(cbuf *chain, int trunc_bytes);
int cbuf_truncate_tail(cbuf *chain, int trunc_bytes);

#endif