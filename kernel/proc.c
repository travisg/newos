#include <string.h>
#include <kernel.h>
#include <stage2.h>
#include <debug.h>
#include <proc.h>
#include <thread.h>
#include <khash.h>

static void *proc_hash = NULL;
static struct proc *kernel_proc = NULL;
static proc_id next_proc_id = 0;

static int proc_struct_compare(void *_p, void *_key)
{
	struct proc *p = _p;
	struct proc_key *key = _key;
	
	if(p->id == key->id) return 0;
	else return 1;
}

static int proc_struct_hash(void *_p, void *_key, int range)
{
	struct proc *p = _p;
	struct proc_key *key = _key;
	
	if(p != NULL) 
		return (p->id % range);
	else
		return (key->id % range);
}

int proc_init(kernel_args *ka)
{
	TOUCH(ka);
	
	dprintf("proc_init: entry\n");
	
	// create the process hash table
	proc_hash = hash_init(15, (addr)&kernel_proc->next - (addr)kernel_proc,
		&proc_struct_compare, &proc_struct_hash);

	// create the kernel process
	kernel_proc = (struct proc *)kmalloc(sizeof(struct proc));
	kernel_proc->name = (char *)kmalloc(strlen("kernel_proc") + 1);
	strcpy(kernel_proc->name, "kernel_proc");
	kernel_proc->id = next_proc_id++;
	kernel_proc->aspace = vm_get_kernel_aspace(); // already created
	kernel_proc->thread_list = 0;

	// stick it in the process hash
	hash_insert(proc_hash, kernel_proc);

	return 0;
}

struct proc *proc_get_kernel_proc()
{
	return kernel_proc;
}

