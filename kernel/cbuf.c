#include <kernel/kernel.h>
#include <kernel/heap.h>
#include <kernel/sem.h>
#include <kernel/smp.h>
#include <kernel/int.h>
#include <kernel/debug.h>
#include <kernel/cbuf.h>
#include <kernel/arch/cpu.h>
#include <sys/errors.h>

#include <libc/string.h>

#define ALLOCATE_CHUNK (PAGE_SIZE / CBUF_LEN)

static cbuf *cbuf_free_list;
static sem_id free_list_sem;
static cbuf *cbuf_free_noblock_list;
static int noblock_spin;

static void initialize_cbuf(cbuf *buf)
{
	buf->len = sizeof(buf->dat);
	buf->data = buf->dat;
	buf->flags = 0;
	buf->total_len = 0;
}

static void *_cbuf_alloc(int size)
{
	return kmalloc(size);
}

static cbuf *allocate_cbuf_mem(int count)
{
	void *_buf;
	cbuf *buf;
	int i;

	count = PAGE_ALIGN(count * CBUF_LEN) / CBUF_LEN;
	_buf = _cbuf_alloc(count * CBUF_LEN);
	if(_buf == NULL)
		return NULL;

	buf = (cbuf *)_buf;
	for(i=0; i<count; i++) {
		initialize_cbuf(buf);
		buf->next = buf + 1;
		if(i == 0) {
			buf->flags |= CBUF_FLAG_CHAIN_HEAD;
			buf->total_len = count * sizeof(buf->dat);
		}
		if(i == count - 1) {
			buf->next = NULL;
			buf->flags |= CBUF_FLAG_CHAIN_TAIL;
		}
		buf++;
	}

	buf = (cbuf *)_buf;
	return buf;
}

static void _clear_chain(cbuf *head, cbuf **tail)
{
	cbuf *buf;
	
	buf = head;
	*tail = NULL;
	while(buf) {
		initialize_cbuf(buf); // doesn't touch the next ptr
		*tail = buf;
		buf = buf->next;
	}

	return;
}

void cbuf_free_chain_noblock(cbuf *buf)
{
	cbuf *head, *last;
	int state;

	if(buf == NULL)
		return;

	head = buf;
	_clear_chain(head, &last);

	state = int_disable_interrupts();
	acquire_spinlock(&noblock_spin);

	last->next = cbuf_free_noblock_list;
	cbuf_free_noblock_list = head;

	release_spinlock(&noblock_spin);
	int_restore_interrupts(state);
}

void cbuf_free_chain(cbuf *buf)
{
	cbuf *head, *last;

	if(buf == NULL)
		return;

	head = buf;
	_clear_chain(head, &last);

	sem_acquire(free_list_sem, 1);

	last->next = cbuf_free_list;
	cbuf_free_list = head;

	sem_release(free_list_sem, 1);
}

cbuf *cbuf_get_chain(int len)
{
	cbuf *chain = NULL;
	cbuf *tail = NULL;
	cbuf *temp;
	int chain_len = 0;

	sem_acquire(free_list_sem, 1);

	while(chain_len < len) {
		if(cbuf_free_list == NULL) {
			// we need to allocate some more cbufs
			sem_release(free_list_sem, 1);
			temp = allocate_cbuf_mem(ALLOCATE_CHUNK);
			cbuf_free_chain(temp);
			sem_acquire(free_list_sem, 1);
			continue;
		}
		
		temp = cbuf_free_list;
		cbuf_free_list = cbuf_free_list->next;
		temp->next = chain;
		if(chain == NULL)
			tail = temp;
		chain = temp;
		
		chain_len += chain->len;
	}
	sem_release(free_list_sem, 1);

	// now we have a chain, fixup the first and last entry
	chain->total_len = len;
	chain->flags |= CBUF_FLAG_CHAIN_HEAD;
	tail->len -= chain_len - len;
	tail->flags |= CBUF_FLAG_CHAIN_TAIL;

	return chain;
}

cbuf *cbuf_get_chain_noblock(int len)
{
	cbuf *chain = NULL;
	cbuf *tail = NULL;
	cbuf *temp;
	int chain_len = 0;
	int state;

	state = int_disable_interrupts();
	acquire_spinlock(&noblock_spin);

	while(chain_len < len) {
		if(cbuf_free_noblock_list == NULL) {
			dprintf("cbuf_get_chain_noblock: not enough cbufs\n");
			release_spinlock(&noblock_spin);
			int_restore_interrupts(state);

			if(chain != NULL)
				cbuf_free_chain_noblock(chain);

			return NULL;
		}
		
		temp = cbuf_free_noblock_list;
		cbuf_free_noblock_list = cbuf_free_noblock_list->next;
		temp->next = chain;
		if(chain == NULL)
			tail = temp;
		chain = temp;
		
		chain_len += chain->len;
	}
	release_spinlock(&noblock_spin);
	int_restore_interrupts(state);

	// now we have a chain, fixup the first and last entry
	chain->total_len = len;
	chain->flags |= CBUF_FLAG_CHAIN_HEAD;
	tail->len -= chain_len - len;
	tail->flags |= CBUF_FLAG_CHAIN_TAIL;

	return chain;
}

int cbuf_memcpy_to_chain(cbuf *chain, int offset, const void *_src, int len)
{
	cbuf *buf;
	char *src = (char *)_src;
	int buf_offset;

	if((chain->flags & CBUF_FLAG_CHAIN_HEAD) == 0) {
		dprintf("cbuf_memcpy_to_chain: chain at 0x%x not head\n", chain);
		return ERR_INVALID_ARGS;
	}

	if(len + offset > chain->total_len) {
		dprintf("cbuf_memcpy_to_chain: len + offset > size of cbuf chain\n");
		return ERR_INVALID_ARGS;
	}

	// find the starting cbuf in the chain to copy to
	buf = chain;
	buf_offset = 0;
	while(offset > 0) {
		if(buf == NULL) {
			dprintf("cbuf_memcpy_to_chain: end of chain reached too early!\n");
			return ERR_GENERAL;
		}
		if(offset < buf->len) {
			// this is the one
			buf_offset = offset;
			break;
		}
		offset -= buf->len;
		buf = buf->next;
	}

	while(len > 0) {
		int to_copy;

		if(buf == NULL) {
			dprintf("cbuf_memcpy_to_chain: end of chain reached too early!\n");
			return ERR_GENERAL;
		}
		to_copy = min(len, buf->len - buf_offset);
		memcpy((char *)buf->data + buf_offset, src, to_copy);

		buf_offset = 0;
		len -= to_copy;
		src += to_copy;
		buf = buf->next;
	}

	return NO_ERROR;
}

int cbuf_memcpy_from_chain(void *_dest, cbuf *chain, int offset, int len)
{
	cbuf *buf;
	char *dest = (char *)_dest;
	int buf_offset;

	if((chain->flags & CBUF_FLAG_CHAIN_HEAD) == 0) {
		dprintf("cbuf_memcpy_from_chain: chain at 0x%x not head\n", chain);
		return ERR_INVALID_ARGS;
	}

	if(len + offset > chain->total_len) {
		dprintf("cbuf_memcpy_from_chain: len + offset > size of cbuf chain\n");
		return ERR_INVALID_ARGS;
	}

	// find the starting cbuf in the chain to copy from
	buf = chain;
	buf_offset = 0;
	while(offset > 0) {
		if(buf == NULL) {
			dprintf("cbuf_memcpy_from_chain: end of chain reached too early!\n");
			return ERR_GENERAL;
		}
		if(offset < buf->len) {
			// this is the one
			buf_offset = offset;
			break;
		}
		offset -= buf->len;
		buf = buf->next;
	}

	while(len > 0) {
		int to_copy;
		
		if(buf == NULL) {
			dprintf("cbuf_memcpy_from_chain: end of chain reached too early!\n");
			return ERR_GENERAL;
		}
		
		to_copy = min(len, buf->len - buf_offset);
		memcpy(dest, (char *)buf->data + buf_offset, to_copy);

		buf_offset = 0;
		len -= to_copy;
		dest += to_copy;
		buf = buf->next;
	}

	return NO_ERROR;
}

cbuf *cbuf_merge_chains(cbuf *chain1, cbuf *chain2)
{
	cbuf *buf;

	if(!chain1 && !chain2)
		return NULL;
	if(!chain1)
		return chain2;
	if(!chain2)
		return chain1;

	if((chain1->flags & CBUF_FLAG_CHAIN_HEAD) == 0) {
		dprintf("cbuf_merge_chain: chain at 0x%x not head\n", chain1);
		return NULL;
	}

	if((chain2->flags & CBUF_FLAG_CHAIN_HEAD) == 0) {
		dprintf("cbuf_merge_chain: chain at 0x%x not head\n", chain2);
		return NULL;
	}

	// walk to the end of the first chain and tag the second one on
	buf = chain1;
	while(buf->next)
		buf = buf->next;

	buf->next = chain2;

	// modify the flags on the chain headers
	buf->flags &= ~CBUF_FLAG_CHAIN_TAIL;
	chain1->total_len += chain2->total_len;
	chain2->flags &= ~CBUF_FLAG_CHAIN_HEAD;

	return chain1;
}

int cbuf_get_len(cbuf *buf)
{
	if(!buf)
		return ERR_INVALID_ARGS;

	if(buf->flags & CBUF_FLAG_CHAIN_HEAD) {
		return buf->total_len;
	} else {
		int len = 0;
		while(buf) {
			len += buf->len;
			buf = buf->next;
		}
		return len;
	}
}

void *cbuf_get_ptr(cbuf *buf, int offset)
{
	while(buf && offset >= 0) {
		if(buf->len > offset)
			return (void *)((int)buf->data + offset);
		offset -= buf->len;
		buf = buf->next;
	}
	return NULL;
}

int cbuf_is_contig_region(cbuf *buf, int start, int end)
{
	while(buf && start >= 0) {
		if(buf->len > start) {
			if(buf->len - start >= end)
				return 1;
			else
				return 0;
		}
		start -= buf->len;
		end -= buf->len;
		buf = buf->next;
	}
	return 0;
}

int cbuf_truncate_head(cbuf *buf, int trunc_bytes)
{
	cbuf *head = buf;

	if(!buf)
		return ERR_INVALID_ARGS;
	if((buf->flags & CBUF_FLAG_CHAIN_HEAD) == 0)
		return ERR_INVALID_ARGS;

	while(buf && trunc_bytes > 0) {
		int to_trunc;

		to_trunc = min(trunc_bytes, buf->len);

		buf->len -= to_trunc;
		buf->data = (void *)((int)buf->data + to_trunc);
		
		trunc_bytes -= to_trunc;
		head->total_len -= to_trunc;
		buf = buf->next;
	}

	return 0;
}

int cbuf_truncate_tail(cbuf *buf, int trunc_bytes)
{
	int offset;
	int buf_offset;
	cbuf *head = buf;

	if(!buf)
		return ERR_INVALID_ARGS;
	if((buf->flags & CBUF_FLAG_CHAIN_HEAD) == 0)
		return ERR_INVALID_ARGS;

	offset = buf->total_len - trunc_bytes;
	buf_offset = 0;
	while(buf && offset > 0) {
		if(offset < buf->len) {
			// this is the one
			buf_offset = offset;
			break;
		}
		offset -= buf->len;
		buf = buf->next;
	}
	if(!buf)
		return ERR_GENERAL;

	head->total_len -= buf->len - buf_offset;
	buf->len -= buf->len - buf_offset;

	// clear out the rest of the buffers in this chain
	buf = buf->next;
	while(buf) {
		head->total_len -= buf->len;
		buf->len = 0;
		buf = buf->next;
	}

	return NO_ERROR;
}

int cbuf_init()
{
	cbuf *buf;

	cbuf_free_list = NULL;
	cbuf_free_noblock_list = NULL;
	noblock_spin = 0;

	free_list_sem = sem_create(1, "cbuf_free_list_sem");
	if(free_list_sem < 0)
		return (int)free_list_sem;

	buf = allocate_cbuf_mem(16);
	if(buf == NULL)
		return ERR_NO_MEMORY;
	cbuf_free_chain_noblock(buf);

	buf = allocate_cbuf_mem(16);
	if(buf == NULL)
		return ERR_NO_MEMORY;
	cbuf_free_chain(buf);

	return NO_ERROR;
}
