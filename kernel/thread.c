/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/thread.h>
#include <kernel/arch/thread.h>
#include <kernel/khash.h>
#include <kernel/int.h>
#include <kernel/smp.h>
#include <kernel/timer.h>
#include <kernel/arch/cpu.h>
#include <kernel/arch/int.h>
#include <kernel/sem.h>
#include <kernel/vfs.h>
#include <kernel/elf.h>
#include <kernel/heap.h>
#include <sys/errors.h>
#include <boot/stage2.h>
#include <libc/string.h>
#include <libc/printf.h>

struct proc_key {
	proc_id id;
};

struct thread_key {
	thread_id id;
};

static struct proc *create_proc_struct(const char *name, bool kernel);
static int proc_struct_compare(void *_p, void *_key);
static unsigned int proc_struct_hash(void *_p, void *_key, int range);

// global
int thread_spinlock = 0;

// proc list
static void *proc_hash = NULL;
static struct proc *kernel_proc = NULL;
static proc_id next_proc_id = 0;
static int proc_spinlock = 0;
	// NOTE: PROC lock can be held over a THREAD lock acquisition, 
	// but not the other way (to avoid deadlock)
#define GRAB_PROC_LOCK() acquire_spinlock(&proc_spinlock)
#define RELEASE_PROC_LOCK() release_spinlock(&proc_spinlock)

// scheduling timer
#define LOCAL_CPU_TIMER timers[smp_get_current_cpu()]
static struct timer_event *timers = NULL;

// thread list
#define CURR_THREAD cur_thread[smp_get_current_cpu()]
static struct thread **cur_thread = NULL;
static void *thread_hash = NULL;
static thread_id next_thread_id = 0;

static sem_id snooze_sem = -1;

// death stacks
// used temporarily as a thread cleans itself up
struct death_stack {
	region_id rid;
	addr address;
	bool in_use;
};
static struct death_stack *death_stacks;
static unsigned int num_death_stacks;
static unsigned int num_free_death_stacks;
static sem_id death_stack_sem;
static int death_stack_spinlock;

// thread queues
static struct thread_queue run_q[THREAD_NUM_PRIORITY_LEVELS] = { { NULL, NULL }, };
static struct thread_queue dead_q;

static int _rand();
static void thread_entry(void);
static struct thread *thread_get_thread_struct_locked(thread_id id);
static struct proc *proc_get_proc_struct(proc_id id);
static struct proc *proc_get_proc_struct_locked(proc_id id);
static void thread_kthread_exit();
static void deliver_signal(struct thread *t, int signal);

// insert a thread onto the tail of a queue	
void thread_enqueue(struct thread *t, struct thread_queue *q)
{
	t->q_next = NULL;
	if(q->head == NULL) {
		q->head = t;
		q->tail = t;
	} else {
		q->tail->q_next = t;
		q->tail = t;
	}
}

struct thread *thread_lookat_queue(struct thread_queue *q)
{
	return q->head;
}

struct thread *thread_dequeue(struct thread_queue *q)
{
	struct thread *t;

	t = q->head;
	if(t != NULL) {
		q->head = t->q_next;
		if(q->tail == t)
			q->tail = NULL;
	}
	return t;
}

struct thread *thread_dequeue_id(struct thread_queue *q, thread_id thr_id)
{
	struct thread *t;
	struct thread *last = NULL;

	t = q->head;
	while(t != NULL) {
		if(t->id == thr_id) {
			if(last == NULL) {
				q->head = t->q_next;
			} else {
				last->q_next = t->q_next;
			}
			if(q->tail == t)
				q->tail = last;
			break;
		}
		last = t;
		t = t->q_next;
	}
	return t;
}

struct thread *thread_lookat_run_q(int priority)
{
	return thread_lookat_queue(&run_q[priority]);
}

void thread_enqueue_run_q(struct thread *t)
{
	// these shouldn't exist
	if(t->priority > THREAD_MAX_PRIORITY)
		t->priority = THREAD_MAX_PRIORITY;
	if(t->priority < 0)
		t->priority = 0;

	thread_enqueue(t, &run_q[t->priority]);
}

struct thread *thread_dequeue_run_q(int priority)
{
	return thread_dequeue(&run_q[priority]);
}

static void insert_thread_into_proc(struct proc *p, struct thread *t)
{
	t->proc_next = p->thread_list;
	p->thread_list = t;
	p->num_threads++;
	if(p->num_threads == 1) {
		// this was the first thread
		p->main_thread = t;
	}
	t->proc = p;
}

static void remove_thread_from_proc(struct proc *p, struct thread *t)
{
	struct thread *temp, *last = NULL;
	
	for(temp = p->thread_list; temp != NULL; temp = temp->proc_next) {
		if(temp == t) {
			if(last == NULL) {
				p->thread_list = temp->proc_next;
			} else {
				last->proc_next = temp->proc_next;
			}
			p->num_threads--;
			break;
		}
		last = temp;
	}
}

static int thread_struct_compare(void *_t, void *_key)
{
	struct thread *t = _t;
	struct thread_key *key = _key;
	
	if(t->id == key->id) return 0;
	else return 1;
}

static unsigned int thread_struct_hash(void *_t, void *_key, int range)
{
	struct thread *t = _t;
	struct thread_key *key = _key;
	
	if(t != NULL) 
		return (t->id % range);
	else
		return (key->id % range);
}

static struct thread *create_thread_struct(const char *name)
{
	struct thread *t;
	int state;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();
	t = thread_dequeue(&dead_q);
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	if(t == NULL) {
		t = (struct thread *)kmalloc(sizeof(struct thread));
		if(t == NULL)
			goto err;
	}
	t->name = (char *)kmalloc(strlen(name) + 1);
	if(t->name == NULL)
		goto err1;
	strcpy(t->name, name);
	t->id = atomic_add(&next_thread_id, 1);
	t->proc = NULL;
	t->kernel_stack_region_id = -1;
	t->kernel_stack_base = 0;
	t->user_stack_region_id = -1;
	t->user_stack_base = 0;
	t->proc_next = NULL;
	t->q_next = NULL;
	t->priority = -1;
	t->args = NULL;
	t->pending_signals = SIG_NONE;
	t->in_kernel = true;
	{
		char temp[64];
		
		sprintf(temp, "thread_0x%x_retcode_sem", t->id);
		t->return_code_sem = sem_create(0, temp);
		if(t->return_code_sem < 0)
			goto err2;
	}

	if(arch_thread_init_thread_struct(t) < 0)
		goto err3;
	
	return t;

err3:
	sem_delete_etc(t->return_code_sem, -1);
err2:
	kfree(t->name);
err1:
	kfree(t);
err:
	return NULL;
}

static int _create_user_thread_kentry(void)
{
	struct thread *t;

	t = thread_get_current_thread();

	// a signal may have been delivered here
	thread_atkernel_exit();		

	// jump to the entry point in user space
	arch_thread_enter_uspace((addr)t->args, t->user_stack_base + STACK_SIZE);

	// never get here
	return 0;
}

static thread_id _create_thread(const char *name, proc_id pid, int priority, addr entry, bool kernel)
{
	struct thread *t;
	struct proc *p;
	int state;
	char stack_name[64];
	bool abort = false;
	
	t = create_thread_struct(name);
	if(t == NULL)
		return ERR_NO_MEMORY;

	t->priority = priority;
	t->state = THREAD_STATE_BIRTH;
	t->next_state = THREAD_STATE_SUSPENDED;

	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	// insert into global list
	hash_insert(thread_hash, t);
	RELEASE_THREAD_LOCK();

	GRAB_PROC_LOCK();
	// look at the proc, make sure it's not being deleted
	p = proc_get_proc_struct_locked(pid);
	if(p != NULL && p->state != PROC_STATE_DEATH) {
		insert_thread_into_proc(p, t);
	} else {
		abort = true;
	}
	RELEASE_PROC_LOCK();
	if(abort) {
		GRAB_THREAD_LOCK();
		hash_remove(thread_hash, t);
		RELEASE_THREAD_LOCK();
	}
	int_restore_interrupts(state);
	if(abort) {
		kfree(t->name);
		kfree(t);
		return ERR_TASK_PROC_DELETED;
	}
	
	sprintf(stack_name, "%s_kstack", name);
	t->kernel_stack_region_id = vm_create_anonymous_region(vm_get_kernel_aspace_id(), stack_name,
		(void **)&t->kernel_stack_base, REGION_ADDR_ANY_ADDRESS, KSTACK_SIZE,
		REGION_WIRING_WIRED, LOCK_RW|LOCK_KERNEL);
	if(t->kernel_stack_region_id < 0)
		panic("_create_thread: error creating kernel stack!\n");

	if(kernel) {
		// this sets up an initial kthread stack that runs the entry
		arch_thread_initialize_kthread_stack(t, (void *)entry, &thread_entry, &thread_kthread_exit);
	} else {
		// create user stack
		// XXX make this better. For now just keep trying to create a stack
		// until we find a spot.
		t->user_stack_base = (USER_STACK_REGION  - STACK_SIZE) + USER_STACK_REGION_SIZE;
		while(t->user_stack_base > USER_STACK_REGION) {
			sprintf(stack_name, "%s_stack%d", p->name, t->id);
			t->user_stack_region_id = vm_create_anonymous_region(p->aspace_id, stack_name,
				(void **)&t->user_stack_base,
				REGION_ADDR_ANY_ADDRESS, STACK_SIZE, REGION_WIRING_WIRED, LOCK_RW);
			if(t->user_stack_region_id < 0) {
				t->user_stack_base -= STACK_SIZE;
			} else {
				// we created a region
				break;
			}
		}
		if(t->user_stack_region_id < 0)
			panic("_create_thread: unable to create user stack!\n");

		// copy the user entry over to the args field in the thread struct
		// the function this will call will immediately switch the thread into
		// user space.
		t->args = (void *)entry;
		arch_thread_initialize_kthread_stack(t, &_create_user_thread_kentry, &thread_entry, &thread_kthread_exit);
	}
	
	t->state = THREAD_STATE_SUSPENDED;

	return t->id;
}

thread_id thread_create_user_thread(char *name, proc_id pid, int priority, addr entry)
{
	return _create_thread(name, pid, priority, entry, false);
}

thread_id thread_create_kernel_thread(const char *name, int (*func)(void), int priority)
{
	return _create_thread(name, proc_get_kernel_proc()->id, priority, (addr)func, true);
}

static thread_id thread_create_kernel_thread_etc(const char *name, int (*func)(void), int priority, struct proc *p)
{
	return _create_thread(name, p->id, priority, (addr)func, true);
}

int thread_suspend_thread(thread_id id)
{
	int state;
	struct thread *t;
	int retval;
	bool global_resched = false;

	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	if(CURR_THREAD->id == id) {
		t = CURR_THREAD;
	} else {		
		t = thread_get_thread_struct_locked(id);
	}
	
	if(t != NULL) {
		if(t->proc == kernel_proc) {
			// no way
			retval = ERR_NOT_ALLOWED;
		} else if(t->in_kernel == true) {
			t->pending_signals |= SIG_SUSPEND;
		} else {
			t->next_state = THREAD_STATE_SUSPENDED;
			global_resched = true;
		}
		retval = NO_ERROR;
	} else {
		retval = ERR_INVALID_HANDLE;
	}

	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	if(global_resched) {
		smp_send_broadcast_ici(SMP_MSG_RESCHEDULE, 0, 0, 0, NULL, SMP_MSG_FLAG_SYNC);
	}
	
	return retval;
}

int thread_resume_thread(thread_id id)
{
	int state;
	struct thread *t;
	int retval;

	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	t = thread_get_thread_struct_locked(id);
	if(t != NULL && t->state == THREAD_STATE_SUSPENDED) {	
		t->state = THREAD_STATE_READY;
		t->next_state = THREAD_STATE_READY;

		thread_enqueue_run_q(t);
		retval = NO_ERROR;
	} else {
		retval = ERR_INVALID_HANDLE;
	}
	
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);
	
	return retval;
}

static const char *state_to_text(int state)
{
	switch(state) {
		case THREAD_STATE_READY:
			return "READY";
		case THREAD_STATE_RUNNING:
			return "RUNNING";
		case THREAD_STATE_WAITING:
			return "WAITING";
		case THREAD_STATE_SUSPENDED:
			return "SUSPEND";
		case THREAD_STATE_FREE_ON_RESCHED:
			return "DEATH";
		case THREAD_STATE_BIRTH:
			return "BIRTH";
		default:
			return "UNKNOWN";
	}
}

static struct thread *last_thread_dumped = NULL;

static void _dump_thread_info(struct thread *t)
{
	dprintf("THREAD: 0x%x\n", t);
	dprintf("name: %s\n", t->name);
	dprintf("all_next:    0x%x\nproc_next:  0x%x\nq_next:     0x%x\n",
		t->all_next, t->proc_next, t->q_next);
	dprintf("id:          0x%x\n", t->id);
	dprintf("priority:    0x%x\n", t->priority);
	dprintf("state:       %s\n", state_to_text(t->state));
	dprintf("next_state:  %s\n", state_to_text(t->next_state));
	dprintf("sem_count:   0x%x\n", t->sem_count);
	dprintf("proc:        0x%x\n", t->proc);
	dprintf("kernel_stack_region_id: 0x%x\n", t->kernel_stack_region_id);
	dprintf("kernel_stack_base: 0x%x\n", t->kernel_stack_base);
	dprintf("user_stack_region_id:   0x%x\n", t->user_stack_region_id);
	dprintf("user_stack_base:   0x%x\n", t->user_stack_base);
	dprintf("architecture dependant section:\n");
	arch_thread_dump_info(&t->arch_info);

	last_thread_dumped = t;
}

static void dump_thread_info(int argc, char **argv)
{
	struct thread *t;
	int id = -1;
	unsigned long num;
	struct hash_iterator i;

	if(argc < 2) {
		dprintf("thread: not enough arguments\n");
		return;
	}

	// if the argument looks like a hex number, treat it as such
	if(strlen(argv[1]) > 2 && argv[1][0] == '0' && argv[1][1] == 'x') {
		num = atoul(argv[1]);
		if(num > vm_get_kernel_aspace()->virtual_map.base) {
			// XXX semi-hack
			_dump_thread_info((struct thread *)num);
			return;
		} else {
			id = num;
		}
	}
	
	// walk through the thread list, trying to match name or id
	hash_open(thread_hash, &i);
	while((t = hash_next(thread_hash, &i)) != NULL) {
		if((t->name && strcmp(argv[1], t->name) == 0) || t->id == id) {
			_dump_thread_info(t);
			break;
		}
	}
	hash_close(thread_hash, &i, false);
}

static void dump_thread_list(int argc, char **argv)
{
	struct thread *t;
	struct hash_iterator i;

	hash_open(thread_hash, &i);
	while((t = hash_next(thread_hash, &i)) != NULL) {
		dprintf("0x%x", t);
		if(t->name != NULL)
			dprintf("\t%32s", t->name);
		else
			dprintf("\t%32s", "<NULL>");
		dprintf("\t0x%x", t->id);
		dprintf("\t%16s", state_to_text(t->state));
		dprintf("\t0x%x\n", t->kernel_stack_base);
	}
	hash_close(thread_hash, &i, false);
}

static void dump_next_thread_in_q(int argc, char **argv)
{
	struct thread *t = last_thread_dumped;

	if(t == NULL) {
		dprintf("no thread previously dumped. Examine a thread first.\n");
		return;
	}
	
	dprintf("next thread in queue after thread @ 0x%x\n", t);
	if(t->q_next != NULL) {
		_dump_thread_info(t->q_next);
	} else {
		dprintf("NULL\n");
	}
}

static void dump_next_thread_in_all_list(int argc, char **argv)
{
	struct thread *t = last_thread_dumped;

	if(t == NULL) {
		dprintf("no thread previously dumped. Examine a thread first.\n");
		return;
	}
	
	dprintf("next thread in global list after thread @ 0x%x\n", t);
	if(t->all_next != NULL) {
		_dump_thread_info(t->all_next);
	} else {
		dprintf("NULL\n");
	}
}

static void dump_next_thread_in_proc(int argc, char **argv)
{
	struct thread *t = last_thread_dumped;

	if(t == NULL) {
		dprintf("no thread previously dumped. Examine a thread first.\n");
		return;
	}
	
	dprintf("next thread in proc after thread @ 0x%x\n", t);
	if(t->proc_next != NULL) {
		_dump_thread_info(t->proc_next);
	} else {
		dprintf("NULL\n");
	}
}

int get_death_stack(void)
{
	unsigned int i;
	int state;

	sem_acquire(death_stack_sem, 1);

	// grab the thread lock around the search for a death stack to make sure it doesn't
	// find a death stack that has been returned by a thread that still hasn't been
	// rescheduled for the last time. Localized hack here and put_death_stack_and_reschedule.
	state = int_disable_interrupts();
	acquire_spinlock(&death_stack_spinlock);
	GRAB_THREAD_LOCK();
	release_spinlock(&death_stack_spinlock);

	for(i=0; i<num_death_stacks; i++) {
		if(death_stacks[i].in_use == false) {
			death_stacks[i].in_use = true;
			break;
		}
	}

	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	if(i >= num_death_stacks) {
		panic("get_death_stack: couldn't find free stack!\n");
	}

	dprintf("get_death_stack: returning 0x%x\n", death_stacks[i].address);
	
	return i;
}

static void put_death_stack_and_reschedule(unsigned int index)
{
	dprintf("put_death_stack...: passed %d\n", index);
	
	if(index >= num_death_stacks || death_stacks[index].in_use == false)
		panic("put_death_stack_and_reschedule: passed invalid stack index %d\n", index);
	death_stacks[index].in_use = false;

	// disable the interrupts around the semaphore release to prevent the get_death_stack
	// function from allocating this stack before the reschedule. Kind of a hack, but localized
	// not an easy way around it.
	int_disable_interrupts();
	
	acquire_spinlock(&death_stack_spinlock);
	sem_release_etc(death_stack_sem, 1, SEM_FLAG_NO_RESCHED);	

	GRAB_THREAD_LOCK();
	release_spinlock(&death_stack_spinlock);

	thread_resched();	
}

int thread_init(kernel_args *ka)
{
	struct thread *t;
	unsigned int i;
	
	dprintf("thread_init: entry\n");
	
	// create the process hash table
	proc_hash = hash_init(15, (addr)&kernel_proc->next - (addr)kernel_proc,
		&proc_struct_compare, &proc_struct_hash);

	// create the kernel process
	kernel_proc = create_proc_struct("kernel_proc", true);
	if(kernel_proc == NULL)
		panic("could not create kernel proc!\n");
	kernel_proc->state = PROC_STATE_NORMAL;

	kernel_proc->ioctx = vfs_new_ioctx();
	if(kernel_proc->ioctx == NULL)
		panic("could not create ioctx for kernel proc!\n");

	// stick it in the process hash
	hash_insert(proc_hash, kernel_proc);

	// create the thread hash table
	thread_hash = hash_init(15, (addr)&t->all_next - (addr)t,
		&thread_struct_compare, &thread_struct_hash);

	// zero out the run queues
	memset(run_q, 0, sizeof(run_q));
	
	// zero out the dead thread structure q
	memset(&dead_q, 0, sizeof(dead_q));

	// allocate as many CUR_THREAD slots as there are cpus
	cur_thread = (struct thread **)kmalloc(sizeof(struct thread *) * smp_get_num_cpus());
	if(cur_thread == NULL) {
		panic("error allocating cur_thread slots\n");
		return ERR_NO_MEMORY;
	}
	memset(cur_thread, 0, sizeof(struct thread *) * smp_get_num_cpus());

	// allocate a timer structure per cpu
	timers = (struct timer_event *)kmalloc(sizeof(struct timer_event) * smp_get_num_cpus());
	if(timers == NULL) {
		panic("error allocating scheduling timers\n");
		return ERR_NO_MEMORY;
	}
	memset(timers, 0, sizeof(struct timer_event) * smp_get_num_cpus());
	
	// allocate a snooze sem
	snooze_sem = sem_create(0, "snooze sem");
	if(snooze_sem < 0) {
		panic("error creating snooze sem\n");
		return snooze_sem;
	}

	// create an idle thread for each cpu
	for(i=0; i<ka->num_cpus; i++) {
		char temp[64];
		
		sprintf(temp, "idle_thread%d", i);
		t = create_thread_struct(temp);
		if(t == NULL) {
			panic("error creating idle thread struct\n");
			return ERR_NO_MEMORY;
		}
		t->proc = proc_get_kernel_proc();
		t->priority = THREAD_IDLE_PRIORITY;
		t->state = THREAD_STATE_RUNNING;
		t->next_state = THREAD_STATE_READY;
		sprintf(temp, "idle_thread%d_kstack", i);
		t->kernel_stack_region_id = vm_find_region_by_name(vm_get_kernel_aspace_id(), temp);		
		hash_insert(thread_hash, t);
		insert_thread_into_proc(t->proc, t);
		cur_thread[i] = t;
	}

	// create a set of death stacks
	num_death_stacks = smp_get_num_cpus();
	num_free_death_stacks = smp_get_num_cpus();
	death_stacks = (struct death_stack *)kmalloc(num_death_stacks * sizeof(struct death_stack));
	if(death_stacks == NULL) {
		panic("error creating death stacks\n");
		return ERR_NO_MEMORY;
	}
	{
		char temp[64];
		
		for(i=0; i<num_death_stacks; i++) {
			sprintf(temp, "death_stack%d", i);
			death_stacks[i].rid = vm_create_anonymous_region(vm_get_kernel_aspace_id(), temp,
				(void **)&death_stacks[i].address,
				REGION_ADDR_ANY_ADDRESS, KSTACK_SIZE, REGION_WIRING_WIRED, LOCK_RW|LOCK_KERNEL);
			if(death_stacks[i].rid < 0) {
				panic("error creating death stacks\n");
				return death_stacks[i].rid;
			}
			death_stacks[i].in_use = false;
		}
	}
	death_stack_sem = sem_create(num_death_stacks, "death_stack_noavail_sem");
	death_stack_spinlock = 0;

	// set up some debugger commands
	dbg_add_command(dump_thread_list, "threads", "list all threads");
	dbg_add_command(dump_thread_info, "thread", "list info about a particular thread");
	dbg_add_command(dump_next_thread_in_q, "next_q", "dump the next thread in the queue of last thread viewed");
	dbg_add_command(dump_next_thread_in_all_list, "next_all", "dump the next thread in the global list of the last thread viewed");
	dbg_add_command(dump_next_thread_in_proc, "next_proc", "dump the next thread in the process of the last thread viewed");

	return 0;
}

// this starts the scheduler. Must be run under the context of
// the initial idle thread.
void thread_start_threading()
{
	int state;
	
	// XXX may not be the best place for this
	// invalidate all of the other processors' TLB caches
	smp_send_broadcast_ici(SMP_MSG_GLOBAL_INVL_PAGE, 0, 0, 0, NULL, SMP_MSG_FLAG_SYNC);
	arch_cpu_global_TLB_invalidate();
	
	// start the other processors
	smp_send_broadcast_ici(SMP_MSG_RESCHEDULE, 0, 0, 0, NULL, SMP_MSG_FLAG_ASYNC);

	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();
	
	thread_resched();
	
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);	
}

void thread_snooze(time_t time)
{
	sem_acquire_etc(snooze_sem, 1, SEM_FLAG_TIMEOUT, time, NULL);
}

// this function gets run by a new thread before anything else
static void thread_entry(void)
{
	// simulates the thread spinlock release that would occur if the thread had been
	// rescheded from. The resched didn't happen because the thread is new.
	RELEASE_THREAD_LOCK();
	int_enable_interrupts(); // this essentially simulates a return-from-interrupt
}

// used to pass messages between thread_exit and thread_exit2
struct thread_exit_args {
	struct thread *t;
	region_id old_kernel_stack;
	int int_state;
	unsigned int death_stack;
};

static void thread_exit2(void *_args)
{
	struct thread_exit_args args;
	char *temp;
	
	// copy the arguments over, since the source is probably on the kernel stack we're about to delete
	memcpy(&args, _args, sizeof(struct thread_exit_args));
	
	// restore the interrupts
	int_restore_interrupts(args.int_state);
	
	dprintf("thread_exit2, running on death stack 0x%x\n", args.t->kernel_stack_base);
	
	// delete the old kernel stack region
	dprintf("thread_exit2: deleting old kernel stack id 0x%x for thread 0x%x\n", args.old_kernel_stack, args.t->id);
	vm_delete_region(vm_get_kernel_aspace_id(), args.old_kernel_stack);
	
	dprintf("thread_exit2: freeing name for thid 0x%x\n", args.t->id);

	// delete the name
	temp = args.t->name;
	args.t->name = NULL;
	if(temp != NULL)
		kfree(temp);
	
	dprintf("thread_exit2: removing thread 0x%x from global lists\n", args.t->id);
	
	// remove this thread from all of the global lists
	int_disable_interrupts();
	GRAB_PROC_LOCK();
	remove_thread_from_proc(kernel_proc, args.t);
	RELEASE_PROC_LOCK();
	GRAB_THREAD_LOCK();
	hash_remove(thread_hash, args.t);
	RELEASE_THREAD_LOCK();
	
	dprintf("thread_exit2: done removing thread from lists\n");
	
	// set the next state to be gone. Will return the thread structure to a ready pool upon reschedule
	args.t->next_state = THREAD_STATE_FREE_ON_RESCHED;

	// return the death stack and reschedule one last time
	put_death_stack_and_reschedule(args.death_stack);
	// never get to here
	panic("thread_exit2: made it where it shouldn't have!\n");
}

void thread_exit(int retcode)
{
	int state;
	struct thread *t = CURR_THREAD;
	struct proc *p = t->proc;
	bool delete_proc = false;
	unsigned int death_stack;
	
	dprintf("thread 0x%x exiting w/return code 0x%x\n", t->id, retcode);

	// delete the user stack region first
	if(p->aspace_id >= 0 && t->user_stack_region_id >= 0) {
		region_id rid = t->user_stack_region_id;
		t->user_stack_region_id = -1;
		vm_delete_region(p->aspace_id, rid);
	}

	if(p != kernel_proc) {
		// remove this thread from the current process and add it to the kernel
		// put the thread into the kernel proc until it dies
		state = int_disable_interrupts();
		GRAB_PROC_LOCK();
		remove_thread_from_proc(p, t);
		t->proc = kernel_proc;
		insert_thread_into_proc(kernel_proc, t);
		if(p->main_thread == t) {
			// this was main thread in this process
			delete_proc = true;
			hash_remove(proc_hash, p);
			p->state = PROC_STATE_DEATH;
		}
		RELEASE_PROC_LOCK();
		GRAB_THREAD_LOCK();
		// reschedule, thus making sure this thread is running in the context of the kernel
		thread_resched();
		RELEASE_THREAD_LOCK();
		int_restore_interrupts(state);
		
		dprintf("thread_exit: thread 0x%x now a kernel thread!\n", t->id);
	}

	// delete the process
	if(delete_proc) {
		if(p->num_threads > 0) {
			// there are other threads still in this process,
			// cycle through and signal kill on each of the threads
			// XXX this can be optimized. There's got to be a better solution.
			struct thread *temp_thread;
			
			state = int_disable_interrupts();
			GRAB_PROC_LOCK();
			// we can safely walk the list because of the lock. no new threads can be created
			// because of the PROC_STATE_DEATH flag on the process
			for(temp_thread = p->thread_list; temp_thread; temp_thread = temp_thread->proc_next) {
				thread_kill_thread_nowait(temp_thread->id);
			}
			RELEASE_PROC_LOCK();
			int_restore_interrupts(state);

			// Now wait for all of the threads to die
			// XXX block on a semaphore
			while((volatile int)p->num_threads > 0) {
				thread_snooze(10000); // 10 ms
			}
		}
		vm_delete_aspace(p->aspace_id);
		vfs_free_ioctx(p->ioctx);
		kfree(p);
	}

	// delete the sem that others will use to wait on us and get the retcode
	{
		sem_id s = t->return_code_sem;
		
		t->return_code_sem = -1;	
		sem_delete_etc(s, retcode);
	}

	death_stack = get_death_stack();	
	{
		struct thread_exit_args args;

		args.t = t;
		args.old_kernel_stack = t->kernel_stack_region_id;
		args.death_stack = death_stack;

		// disable the interrupts. Must remain disabled until the kernel stack pointer can be officially switched
		args.int_state = int_disable_interrupts();

		// set the new kernel stack officially to the death stack, wont be really switched until
		// the next function is called. This bookkeeping must be done now before a context switch
		// happens, or the processor will interrupt to the old stack
		t->kernel_stack_region_id = death_stacks[death_stack].rid;
		t->kernel_stack_base = death_stacks[death_stack].address;

		// we will continue in thread_exit2(), on the new stack
		arch_thread_switch_kstack_and_call(t, t->kernel_stack_base + KSTACK_SIZE, thread_exit2, &args);
	}

	panic("never can get here\n");
}

static int _thread_kill_thread(thread_id id, bool wait_on)
{
	int state;
	struct thread *t;
	int rc;

	dprintf("_thread_kill_thread: id %d, wait_on %d\n", id, wait_on);

	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	t = thread_get_thread_struct_locked(id);
	if(t != NULL) {
		if(t->proc == kernel_proc) {
			// can't touch this
			rc = ERR_NOT_ALLOWED;
		} else {
			deliver_signal(t, SIG_KILL);
			rc = NO_ERROR;
			if(t->id == CURR_THREAD->id)
				wait_on = false; // can't wait on ourself
		}
	} else {
		rc = ERR_INVALID_HANDLE;
	}

	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);
	if(rc < 0)
		return rc;

	if(wait_on)
		thread_wait_on_thread(id, NULL);

	return rc;
}

int thread_kill_thread(thread_id id)
{
	return _thread_kill_thread(id, true);
}

int thread_kill_thread_nowait(thread_id id)
{
	return _thread_kill_thread(id, false);
}

static void thread_kthread_exit()
{
	thread_exit(0);
}

int thread_wait_on_thread(thread_id id, int *retcode)
{
	sem_id sem;
	int state;
	struct thread *t;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	t = thread_get_thread_struct_locked(id);
	if(t != NULL) {
		sem = t->return_code_sem;
	} else {
		sem = ERR_INVALID_HANDLE;
	}
	
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	return sem_acquire_etc(sem, 1, 0, 0, retcode);
}

int proc_wait_on_proc(proc_id id, int *retcode)
{
	struct proc *p;
	thread_id tid;
	int state;
	
	state = int_disable_interrupts();
	GRAB_PROC_LOCK();
	p = proc_get_proc_struct_locked(id);
	if(p && p->main_thread) {
		tid = p->main_thread->id;
	} else {
		tid = ERR_INVALID_HANDLE;
	}
	RELEASE_PROC_LOCK();
	int_restore_interrupts(state);

	return thread_wait_on_thread(tid, retcode);
}

thread_id thread_get_current_thread_id()
{
	if(cur_thread == NULL)
		return 0;
	return CURR_THREAD->id;
}

struct thread *thread_get_current_thread()
{
	return CURR_THREAD;
}

struct thread *thread_get_thread_struct(thread_id id)
{
	struct thread *t;
	int state;
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	t = thread_get_thread_struct_locked(id);
	
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);
	
	return t;
}
		
static struct thread *thread_get_thread_struct_locked(thread_id id)
{
	struct thread_key key;
	
	key.id = id;
	
	return hash_lookup(thread_hash, &key);
}

static struct proc *proc_get_proc_struct(proc_id id)
{
	struct proc *p;
	int state;
	
	state = int_disable_interrupts();
	GRAB_PROC_LOCK();

	p = proc_get_proc_struct_locked(id);
	
	RELEASE_PROC_LOCK();
	int_restore_interrupts(state);
	
	return p;
}
		
static struct proc *proc_get_proc_struct_locked(proc_id id)
{
	struct proc_key key;
	
	key.id = id;
	
	return hash_lookup(proc_hash, &key);
}	

static void thread_context_switch(struct thread *t_from, struct thread *t_to)
{
	arch_thread_context_switch(t_from, t_to);
}

#define NUM_TEST_THREADS 16
/* thread TEST code */
static sem_id thread_test_sems[NUM_TEST_THREADS];
static thread_id thread_test_first_thid;

int test_thread_starter_thread()
{
	thread_snooze(1000000); // wait a second
	
	// start the chain of threads by releasing one of them
	sem_release(thread_test_sems[0], 1);

	return 0;	
}

int test_thread5()
{
	int fd;
	
	fd = sys_open("/bus/pci", "", STREAM_TYPE_DEVICE);
	if(fd < 0) {
		dprintf("test_thread5: error opening /bus/pci\n");
		return 1;
	}
	
	sys_ioctl(fd, 99, NULL, 0);

	return 0;
}

int test_thread4()
{
	proc_id pid;
	
	pid = proc_create_proc("/boot/testapp", "testapp", 5);
	if(pid < 0)
		return -1;

	dprintf("test_thread4: finished created new process\n");

	thread_snooze(1000000);
	
	// kill the process
//	proc_kill_proc(pid);

	return 0;
}

int test_thread3()
{
	int fd;
	char buf[1024];
	size_t len;

	kprintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
	fd = sys_open("/boot/testfile", "", STREAM_TYPE_FILE);
	if(fd < 0)
		panic("could not open /boot/testfile\n");
	len = sizeof(buf);
	sys_read(fd, buf, 0, &len);
	sys_write(0, buf, 0, &len);
	sys_close(fd);

	return 0;
}

int test_thread2()
{
	while(1) {
		char str[65];
		size_t len = sizeof(str) - 1;
		
		if(sys_read(0, str, 0, &len) < 0) {
			dprintf("error reading from console!\n");
			break;
		}
		if(len > 1) dprintf("test_thread2: read %d bytes\n", len);
		str[len] = 0;
		kprintf("%s", str);
	}
	return 0;
}

int test_thread()
{
	int a = 0;
	int tid = thread_get_current_thread_id();
	int x, y;
//	char c = 'a';
	
	x = tid % 80;
	y = (tid / 80) * 2 ;
	
	while(1) {
//		a += tid;
		a++;
#if 1
		kprintf_xy(0, tid-1, "thread%d - %d    - 0x%x 0x%x - cpu %d", tid, a, system_time(), smp_get_current_cpu());
#endif
#if 0
		dprintf("thread%d - %d    - %d %d - cpu %d\n", tid, a, system_time(), smp_get_current_cpu());
#endif
#if 0
		kprintf("thread%d - %d    - %d %d - cpu %d\n", tid, a, system_time(), smp_get_current_cpu());
#endif
#if 0
		kprintf_xy(x, y, "%c", c++);
		if(c > 'z')
			c = 'a';
		kprintf_xy(x, y+1, "%d", smp_get_current_cpu());
#endif
#if 0
		thread_snooze(10000 * tid);
#endif
#if 0
		sem_acquire(thread_test_sems[tid - thread_test_first_thid], 1);

		// release the next semaphore
		{
			sem_id sem_to_release;			

			sem_to_release = tid - thread_test_first_thid + 1;
			if(sem_to_release >= NUM_TEST_THREADS)
				sem_to_release = 0;
			sem_to_release = thread_test_sems[sem_to_release];
			sem_release(sem_to_release, 1);
		}	
#endif
#if 0
		switch(tid - thread_test_first_thid) {
			case 2: case 4:
				if((a % 2048) == 0)
					sem_release(thread_test_sem, _rand() % 16 + 1);
				break;
			default:
				sem_acquire(thread_test_sem, 1);
		}
#endif
#if 0
		if(a > tid * 100) {
			kprintf("thread %d exiting\n", tid);
			break;
		}
#endif
	}
	return 1;
}

int panic_thread()
{
	dprintf("panic thread starting\n");
	
	thread_snooze(10000000);
	panic("gotcha!\n");
	return 0;
}

int thread_test()
{
	thread_id tid;
	int i;
	char temp[64];
	
#if 1
	for(i=0; i<NUM_TEST_THREADS; i++) {
		sprintf(temp, "test_thread%d", i);
		tid = thread_create_kernel_thread(temp, &test_thread, 5);
		thread_resume_thread(tid);
		if(i == 0) {
			thread_test_first_thid = tid;
		}
		sprintf(temp, "test sem %d", i);
		thread_test_sems[i] = sem_create(0, temp);
	}	
	tid = thread_create_kernel_thread("test starter thread", &test_thread_starter_thread, THREAD_MAX_PRIORITY);
	thread_resume_thread(tid);
#endif
#if 0
	tid = thread_create_kernel_thread("test thread 2", &test_thread2, 5);
	thread_resume_thread(tid);
#endif
#if 0
	tid = thread_create_kernel_thread("test thread 3", &test_thread3, 5);
	thread_resume_thread(tid);
#endif
#if 0
	tid = thread_create_kernel_thread("test thread 4", &test_thread4, 5);
	thread_resume_thread(tid);
#endif
#if 0
	tid = thread_create_kernel_thread("test thread 5", &test_thread5, 5);
	thread_resume_thread(tid);
#endif
#if 0
	tid = thread_create_kernel_thread("panic thread", &panic_thread, THREAD_MAX_PRIORITY);
	thread_resume_thread(tid);
#endif
	dprintf("thread_test: done creating test threads\n");
	
	return 0;
}

static int _rand()
{
	static int next = 0;

	if(next == 0)
		next = system_time();

	next = next * 1103515245 + 12345;
	return((next >> 16) & 0x7FFF);
}

static int reschedule_event(void *unused)
{
	// this function is called as a result of the timer event set by the scheduler
	// returning this causes a reschedule on the timer event
	return INT_RESCHEDULE;
}

// NOTE: expects thread_spinlock to be held
void thread_resched()
{
	struct thread *next_thread = NULL;
	int last_thread_pri = -1;
	struct thread *old_thread = CURR_THREAD;
	int i;
	time_t quantum;
	
//	dprintf("top of thread_resched: cpu %d, cur_thread = 0x%x\n", smp_get_current_cpu(), CURR_THREAD);
	
	switch(old_thread->next_state) {
		case THREAD_STATE_RUNNING:
		case THREAD_STATE_READY:
//			dprintf("enqueueing thread 0x%x into run q. pri = %d\n", old_thread, old_thread->priority);
			thread_enqueue_run_q(old_thread);
			break;
		case THREAD_STATE_SUSPENDED:
			dprintf("suspending thread 0x%x\n", old_thread->id);
			break;
		case THREAD_STATE_FREE_ON_RESCHED:
			thread_enqueue(old_thread, &dead_q);
			break;
		default:
//			dprintf("not enqueueing thread 0x%x into run q. next_state = %d\n", old_thread, old_thread->next_state);
			;
	}
	old_thread->state = old_thread->next_state;

	for(i = THREAD_MAX_PRIORITY; i > THREAD_IDLE_PRIORITY; i--) {
		next_thread = thread_lookat_run_q(i);
		if(next_thread != NULL) {
			// skip it sometimes
			if(_rand() > 0x3000) {
				next_thread = thread_dequeue_run_q(i);
				break;
			}
			last_thread_pri = i;
			next_thread = NULL;
		}
	}
	if(next_thread == NULL) {
		if(last_thread_pri != -1) {
			next_thread = thread_dequeue_run_q(last_thread_pri);
			if(next_thread == NULL)
				panic("next_thread == NULL! last_thread_pri = %d\n", last_thread_pri);
		} else {
			next_thread = thread_dequeue_run_q(THREAD_IDLE_PRIORITY);
			if(next_thread == NULL)
				panic("next_thread == NULL! no idle priorities!\n", last_thread_pri);
		}
	}

	next_thread->state = THREAD_STATE_RUNNING;
	next_thread->next_state = THREAD_STATE_READY;

	// XXX calculate quantum
	quantum = 10000;

	timer_cancel_event(&LOCAL_CPU_TIMER);
	timer_setup_timer(&reschedule_event, NULL, &LOCAL_CPU_TIMER);
	timer_set_event(quantum, TIMER_MODE_ONESHOT, &LOCAL_CPU_TIMER);

	if(next_thread != old_thread) {
//		dprintf("thread_resched: cpu %d switching from thread %d to %d\n",
//			smp_get_current_cpu(), old_thread->id, next_thread->id);
		CURR_THREAD = next_thread;
		thread_context_switch(old_thread, next_thread);
	}
}

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

struct proc *proc_get_kernel_proc()
{
	return kernel_proc;
}

proc_id proc_get_current_proc_id()
{
	return CURR_THREAD->proc->id;
}

static struct proc *create_proc_struct(const char *name, bool kernel)
{
	struct proc *p;
	
	p = (struct proc *)kmalloc(sizeof(struct proc));
	if(p == NULL)
		goto error;
	p->id = atomic_add(&next_proc_id, 1);
	p->name = (char *)kmalloc(strlen(name)+1);
	if(p->name == NULL)
		goto error1;
	strcpy(p->name, name);
	p->num_threads = 0;
	p->ioctx = NULL;
	p->aspace_id = -1;
	p->aspace = NULL;
	p->kaspace = vm_get_kernel_aspace();
	p->thread_list = NULL;
	p->main_thread = NULL;
	p->state = PROC_STATE_BIRTH;
	p->pending_signals = SIG_NONE;
	p->proc_creation_sem = sem_create(0, "proc_creation_sem");
	if(p->proc_creation_sem < 0)
		goto error2;
	if(arch_proc_init_proc_struct(p, kernel) < 0)
		goto error3;

	return p;

error3:
	sem_delete(p->proc_creation_sem);
error2:
	kfree(p->name);
error1:
	kfree(p);
error:
	return NULL;
}

static int proc_create_proc2(void)
{
	int err;
	struct thread *t;
	struct proc *p;
	char *path;
	addr entry;
	char ustack_name[128];

	t = thread_get_current_thread();
	p = t->proc;

	dprintf("proc_create_proc2: entry thread %d\n", t->id);

	// create an initial primary stack region
	t->user_stack_base = ((USER_STACK_REGION  - STACK_SIZE) + USER_STACK_REGION_SIZE);
	sprintf(ustack_name, "%s_primary_stack", p->name);
	t->user_stack_region_id = vm_create_anonymous_region(p->aspace_id, ustack_name, (void **)&t->user_stack_base,
		REGION_ADDR_EXACT_ADDRESS, STACK_SIZE, REGION_WIRING_LAZY, LOCK_RW);
	if(t->user_stack_region_id < 0) {
		panic("proc_create_proc2: could not create default user stack region\n");
		sem_delete_etc(p->proc_creation_sem, -1);
		return t->user_stack_region_id;
	}

	path = p->args;
	dprintf("proc_create_proc2: loading elf binary '%s'\n", path);

	err = elf_load(path, p, 0, &entry);
	if(err < 0){
		// XXX clean up proc
		sem_delete_etc(p->proc_creation_sem, -1);
		return err;
	}

	dprintf("proc_create_proc2: loaded elf. entry = 0x%x\n", entry);

	p->state = PROC_STATE_NORMAL;

	// this will wake up the thread that initially created us, with the process id
	// as the return code
	sem_delete_etc(p->proc_creation_sem, p->id);
	p->proc_creation_sem = 0;

	// jump to the entry point in user space
	arch_thread_enter_uspace(entry, t->user_stack_base + STACK_SIZE);

	// never gets here
	return 0;
}

proc_id proc_create_proc(const char *path, const char *name, int priority)
{
	struct proc *p;
	thread_id tid;
	int err;
	unsigned int state;
	int sem_retcode;

	dprintf("proc_create_proc: entry '%s', name '%s'\n", path, name);

	p = create_proc_struct(name, false);
	if(p == NULL)
		return ERR_NO_MEMORY;

	state = int_disable_interrupts();
	GRAB_PROC_LOCK();
	hash_insert(proc_hash, p);
	RELEASE_PROC_LOCK();
	int_restore_interrupts(state);

	// create a kernel thread, but under the context of the new process
	tid = thread_create_kernel_thread_etc(name, proc_create_proc2, priority, p);
	if(tid < 0) {
		// XXX clean up proc
		return tid;
	}
	
	// copy the args over
	p->args = kmalloc(strlen(path) + 1);
	if(p->args == NULL) {
		// XXX clean up proc
		return ERR_NO_MEMORY;
	}
	strcpy(p->args, path);

	// create a new ioctx for this process
	p->ioctx = vfs_new_ioctx();
	if(p->ioctx == NULL) {
		// XXX clean up proc
		panic("proc_create_proc: could not create new ioctx\n");
		return ERR_NO_MEMORY;
	}

	// create an address space for this process
	p->aspace_id = vm_create_aspace(p->name, USER_BASE, USER_SIZE, false);
	if(p->aspace_id < 0) {
		// XXX clean up proc
		panic("proc_create_proc: could not create user address space\n");
		return p->aspace_id;
	}
	p->aspace = vm_get_aspace_from_id(p->aspace_id);

	thread_resume_thread(tid);
	
	// XXX race condition
	// acquire this semaphore, which will exist throughout the creation of the process
	// by the new thread in the new process. At the end of creation, the semaphore will
	// be deleted, with the return code being the process id, or an error.
	sem_acquire_etc(p->proc_creation_sem, 1, 0, 0, &sem_retcode);
	
	// this will either contain the process id, or an error code
	return sem_retcode;
}

int proc_kill_proc(proc_id id)
{
	int state;
	struct proc *p;
	struct thread *t;
	thread_id tid = -1;
	int retval = 0;
	
	state = int_disable_interrupts();
	GRAB_PROC_LOCK();
	
	p = proc_get_proc_struct_locked(id);
	if(p != NULL) {
		tid = p->main_thread->id;
	} else {
		retval = ERR_INVALID_HANDLE;
	}

	RELEASE_PROC_LOCK();
	int_restore_interrupts(state);
	if(retval < 0)
		return retval;

	// just kill the main thread in the process. The cleanup code there will
	// take care of the process
	return thread_kill_thread(tid);
#if 0
	// now suspend all of the threads in this process. It's safe to walk this process's
	// thread list without the lock held because the state of this process is now DEATH
	// so all of the operations that involve changing the thread list are blocked
	// also, it's ok to 'suspend' this thread, if we belong to this process, since we're
	// in the kernel now, we won't be suspended until we leave the kernel. By then,
	// we will have passed the kill signal to this thread.
	for(t = p->thread_list; t; t = t->proc_next) {
		thread_suspend_thread(t->id);
	}

	// XXX cycle through the list of threads again, killing each thread.
	// Note: this wont kill the current thread, if it's one of them, since we're in the
	// kernel.
	// If we actually kill the last thread and not just deliver a signal to it, remove
	// the process along with it, otherwise the last thread that belongs to the process
	// will clean it up when it dies.

	// XXX not finished
#endif
	return retval;
}

// sets the pending signal flag on a thread and possibly does some work to wake it up, etc.
// expects the thread lock to be held
static void deliver_signal(struct thread *t, int signal)
{
	dprintf("deliver_signal: thread 0x%x (%d), signal %d\n", t, t->id, signal);
	switch(signal) {
		case SIG_KILL:
			t->pending_signals |= SIG_KILL;
			switch(t->state) {
				case THREAD_STATE_SUSPENDED:
					t->state = THREAD_STATE_READY;
					t->next_state = THREAD_STATE_READY;

					thread_enqueue_run_q(t);				
					break;
				case THREAD_STATE_WAITING:
					// XXX wake it up
				default:
					;
			}
			break;
		default:
			t->pending_signals |= signal;
	}
}

// expects the thread lock to be held
static void _check_for_thread_sigs(struct thread *t, int state)
{
	if(t->pending_signals == SIG_NONE)
		return;

	if(t->pending_signals & SIG_KILL) {
		t->pending_signals &= ~SIG_KILL;

		RELEASE_THREAD_LOCK();
		int_restore_interrupts(state);
		thread_exit(0);
		// never gets to here
	}		
	if(t->pending_signals & SIG_SUSPEND) {
		t->pending_signals &= ~SIG_SUSPEND;
		t->next_state = THREAD_STATE_SUSPENDED;
		// XXX will probably want to delay this
		thread_resched();
	}
}

// called in the int handler code when a thread enters the kernel for any reason
void thread_atkernel_entry()
{
	int state;
	struct thread *t = CURR_THREAD;

//	dprintf("thread_atkernel_entry: entry thread 0x%x\n", t->id);

	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();
	
	t->in_kernel = true;
	
	_check_for_thread_sigs(t, state);
	
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);
}

// called when a thread exits kernel space to user space
void thread_atkernel_exit()
{
	int state;
	struct thread *t = CURR_THREAD;

//	dprintf("thread_atkernel_exit: entry\n");

	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	t->in_kernel = false;

	_check_for_thread_sigs(t, state);
	
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);
}
