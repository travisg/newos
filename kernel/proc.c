#include <string.h>
#include <kernel.h>
#include <stage2.h>
#include <debug.h>
#include <proc.h>
#include <thread.h>
#include <khash.h>
#include <vfs.h>
#include <smp.h>
#include <arch_cpu.h>

static void *proc_hash = NULL;
static struct proc *kernel_proc = NULL;
static proc_id next_proc_id = 0;

static struct proc *create_proc_struct(const char *name);

static int proc_struct_compare(void *_p, void *_key)
{
	struct proc *p = _p;
	struct proc_key *key = _key;
	
	if(p->id == key->id) return 0;
	else return 1;
}

static unsigned int proc_struct_hash(void *_p, void *_key, int range)
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
	kernel_proc = create_proc_struct("kernel_proc");
	if(kernel_proc == NULL)
		panic("could not create kernel proc!\n");

	// stick it in the process hash
	hash_insert(proc_hash, kernel_proc);

	return 0;
}

struct proc *proc_get_kernel_proc()
{
	return kernel_proc;
}

static struct proc *create_proc_struct(const char *name)
{
	struct proc *p;
	
	p = (struct proc *)kmalloc(sizeof(struct proc));
	if(p == NULL)
		return NULL;
	p->id = atomic_add(&next_proc_id, 1);
	p->name = (char *)kmalloc(strlen(name)+1);
	strcpy(p->name, name);
	p->ioctx = vfs_new_ioctx();
	p->cwd = NULL;
	p->kaspace = vm_get_kernel_aspace();
	p->aspace = NULL;
	p->thread_list = NULL;
	return p;
}

struct proc *proc_create_user_proc(const char *name, struct proc *parent, int priority)
{
	struct proc *p;
	struct thread *t;
	
	p = create_proc_struct(name);
	if(p == NULL)
		return NULL;
	p->aspace = vm_create_aspace(name, USER_BASE, USER_SIZE);
	if(p->aspace == NULL) {
		// XXX clean up proc
		return NULL;
	}
	
	hash_insert(proc_hash, p);
	
	// create an initial thread
	t = thread_create_user_thread("main thread", p, priority);
	
	return p;
}
