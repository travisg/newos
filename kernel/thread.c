#include <string.h>
#include <printf.h>
#include <kernel/kernel.h>
#include <boot/stage2.h>
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
static thread_id next_thread_id = 0;

// proc list
static void *proc_hash = NULL;
static struct proc *kernel_proc = NULL;
static proc_id next_proc_id = 0;
static int proc_spinlock = 0;
#define GRAB_PROC_LOCK() acquire_spinlock(&proc_spinlock)
#define RELEASE_PROC_LOCK() release_spinlock(&proc_spinlock)

// scheduling timer
#define LOCAL_CPU_TIMER timers[smp_get_current_cpu()]
static struct timer_event *timers = NULL;

// thread list
#define CURR_THREAD cur_thread[smp_get_current_cpu()]
static struct thread **cur_thread = NULL;
static void *thread_hash = NULL;

static sem_id snooze_sem = -1;

// thread queues
static struct thread_queue run_q[THREAD_NUM_PRIORITY_LEVELS] = { { NULL, NULL }, };
static struct thread_queue death_q = { NULL, NULL };

static int _rand();
static void thread_entry(void);

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
	struct proc *t = _t;
	struct proc_key *key = _key;
	
	if(t != NULL) 
		return (t->id % range);
	else
		return (key->id % range);
}

static struct thread *create_thread_struct(const char *name)
{
	struct thread *t;
	
	t = (struct thread *)kmalloc(sizeof(struct thread));
	if(t == NULL)
		goto err;
	t->name = (char *)kmalloc(strlen(name) + 1);
	if(t->name == NULL)
		goto err1;
	strcpy(t->name, name);
	t->id = atomic_add(&next_thread_id, 1);
	t->proc = NULL;
	t->kernel_stack_area = NULL;
	t->user_stack_area = NULL;
	t->proc_next = NULL;
	t->q_next = NULL;
	t->priority = -1;

	if(arch_thread_init_thread_struct(t) < 0)
		goto err2;
	
	return t;

err2:
	kfree(t->name);
err1:
	kfree(t);
err:
	return NULL;
}

struct thread *thread_create_user_thread(char *name, struct proc *p, int priority, addr entry)
{
	struct thread *t;
	unsigned int *stack_addr;
	int state;
	char stack_name[64];

	t = create_thread_struct(name);
	if(t == NULL)
		return NULL;
	t->proc = p;
	t->priority = priority;
	t->state = THREAD_STATE_READY;
	t->next_state = THREAD_STATE_READY;

	sprintf(stack_name, "%s_kstack", name);
	vm_create_area(t->proc->kaspace, stack_name, (void **)&stack_addr,
		AREA_ANY_ADDRESS, KSTACK_SIZE, LOCK_RW|LOCK_KERNEL);
	t->kernel_stack_area = vm_find_area_by_name(t->proc->kaspace, stack_name);
	//arch_thread_initialize_kthread_stack(t, func, &thread_entry);

	sprintf(stack_name, "%s_stack", name);
	vm_create_area(t->proc->aspace, stack_name, (void **)&stack_addr,
		AREA_ANY_ADDRESS, STACK_SIZE, LOCK_RW);
	t->kernel_stack_area = vm_find_area_by_name(t->proc->aspace, stack_name);

	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	// insert into global list
	hash_insert(thread_hash, t);

	insert_thread_into_proc(t->proc, t);

	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);
	
	return t;
}

static struct thread *create_kernel_thread(struct proc *p, const char *name, int (*func)(void), int priority)
{
	struct thread *t;
	unsigned int *kstack_addr;
	int state;
	char stack_name[64];
	
	t = create_thread_struct(name);
	if(t == NULL)
		return NULL;
	if(p == NULL)
		p = proc_get_kernel_proc();
	t->proc = p;
	t->priority = priority;
	t->state = THREAD_STATE_READY;
	t->next_state = THREAD_STATE_READY;

	sprintf(stack_name, "%s_kstack", name);
	vm_create_area(t->proc->kaspace, stack_name, (void **)&kstack_addr,
		AREA_ANY_ADDRESS, KSTACK_SIZE, LOCK_RW|LOCK_KERNEL);
	t->kernel_stack_area = vm_find_area_by_name(t->proc->kaspace, stack_name);
	arch_thread_initialize_kthread_stack(t, func, &thread_entry);
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();

	// insert into global list
	hash_insert(thread_hash, t);

	insert_thread_into_proc(t->proc, t);

	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);
	
	return t;
}

int thread_killer()
{
	dprintf("thread_killer: manny is alive!\n");

	while(1) {
		int state;
		struct thread *t;

retry:		
		// kill something
		state = int_disable_interrupts();
		GRAB_THREAD_LOCK();
		
		while((t = thread_lookat_queue(&death_q)) != NULL) {
			if(t->state != THREAD_STATE_MARKED_FOR_DEATH) {
				// the thread is probably still running, hasn't rescheded yet
				RELEASE_THREAD_LOCK();
				int_restore_interrupts(state);
				thread_snooze(10000); // 10ms
				goto retry;
			}
			
			dprintf("manny killing thread %d\n", t->id);	
			t = thread_dequeue(&death_q);
			// XXX kill the thread		
		}				
		
		RELEASE_THREAD_LOCK();
		int_restore_interrupts(state);

		thread_snooze(100000); // 100ms			
	}
	return 0;
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
		case THREAD_STATE_MARKED_FOR_DEATH:
			return "DEATH";
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
	dprintf("blocked_sem: 0x%x\n", t->blocked_sem_id);
	dprintf("proc:        0x%x\n", t->proc);
	dprintf("kernel_stack_area: 0x%x\n", t->kernel_stack_area);
	dprintf("user_stack_area:   0x%x\n", t->user_stack_area);
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
		if(num > vm_get_kernel_aspace()->base) {
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
		if(strcmp(argv[1], t->name) == 0 || t->id == id) {
			_dump_thread_info(t);
			return;
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
		dprintf("0x%x\t%32s\t0x%x\t%16s\t0x%x\n",
			t, t->name, t->id, state_to_text(t->state), t->kernel_stack_area->base);
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

	// stick it in the process hash
	hash_insert(proc_hash, kernel_proc);

	// create the thread hash table
	thread_hash = hash_init(15, (addr)&t->all_next - (addr)t,
		&thread_struct_compare, &thread_struct_hash);

	// zero out the run queues
	memset(run_q, 0, sizeof(run_q));

	// allocate as many CUR_THREAD slots as there are cpus
	cur_thread = (struct thread **)kmalloc(sizeof(struct thread *) * smp_get_num_cpus());
	if(cur_thread == NULL) {
		panic("error allocating cur_thread slots\n");
		return -1;
	}
	memset(cur_thread, 0, sizeof(struct thread *) * smp_get_num_cpus());

	// allocate a timer structure per cpu
	timers = (struct timer_event *)kmalloc(sizeof(struct timer_event) * smp_get_num_cpus());
	if(timers == NULL) {
		panic("error allocating scheduling timers\n");
		return -1;
	}
	memset(timers, 0, sizeof(struct timer_event) * smp_get_num_cpus());
	
	// allocate a snooze sem
	snooze_sem = sem_create(0, "snooze sem");
	if(snooze_sem < 0) {
		panic("error creating snooze sem\n");
		return -1;
	}

	// create an idle thread for each cpu
	for(i=0; i<ka->num_cpus; i++) {
		char temp[64];
		
		sprintf(temp, "idle_thread%d", i);
		t = create_thread_struct(temp);
		if(t == NULL) {
			panic("error creating idle thread struct\n");
			return -1;
		}
		t->proc = proc_get_kernel_proc();
		t->priority = THREAD_IDLE_PRIORITY;
		t->state = THREAD_STATE_RUNNING;
		t->next_state = THREAD_STATE_READY;
		sprintf(temp, "idle_thread%d_kstack", i);
		t->kernel_stack_area = vm_find_area_by_name(t->proc->kaspace, temp);		
		hash_insert(thread_hash, t);
		insert_thread_into_proc(t->proc, t);
		cur_thread[i] = t;
	}

	// create a worker thread to kill other ones
	t = create_kernel_thread(NULL, "manny", &thread_killer, THREAD_MAX_PRIORITY);
	if(t == NULL) {
		panic("error creating Manny the thread killer\n");
		return -1;
	}
	thread_enqueue_run_q(t);

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
	
	// start the other processors
	smp_send_broadcast_ici(SMP_MSG_RESCHEDULE, 0, NULL, SMP_MSG_FLAG_ASYNC);

	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();
	
	thread_resched();
	
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);	
}

void thread_snooze(time_t time)
{
	sem_acquire_etc(snooze_sem, 1, SEM_FLAG_TIMEOUT, time);
}

// this function gets run by a new thread before anything else
static void thread_entry(void)
{
	// simulates the thread spinlock release that would occur if the thread had been
	// rescheded from. The resched didn't happen because the thread is new.
	RELEASE_THREAD_LOCK();
	int_enable_interrupts(); // this essentially simulates a return-from-interrupt
}

void thread_kthread_exit()
{
	int state;
	struct thread *t = CURR_THREAD;
	
	dprintf("kernel thread exiting\n");
	
	state = int_disable_interrupts();
	GRAB_THREAD_LOCK();
	
	// put ourselves into the death queue
	thread_enqueue(t, &death_q);
	t->next_state = THREAD_STATE_MARKED_FOR_DEATH;
	
	thread_resched();

	// will never make it here
}

thread_id thread_get_current_thread_id()
{
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
		
struct thread *thread_get_thread_struct_locked(thread_id id)
{
	struct thread_key key;
	
	key.id = id;
	
	return hash_lookup(thread_hash, &key);
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
	
	fd = vfs_open(NULL, "/bus/pci", "", STREAM_TYPE_DEVICE);
	if(fd < 0) {
		dprintf("test_thread5: error opening /bus/pci\n");
		return 1;
	}
	
	vfs_ioctl(fd, 99, NULL, 0);

	return 0;
}

int test_thread4()
{
	proc_create_proc("/boot/testapp", "testapp", 5);

	return 0;
}

int test_thread3()
{
	int fd;
	char buf[1024];
	size_t len;

	kprintf("\n\n\n");
	fd = vfs_open(NULL, "/boot/testfile", "", STREAM_TYPE_FILE);
	if(fd < 0)
		panic("could not open /boot/testfile\n");
	len = sizeof(buf);
	vfs_read(fd, buf, 0, &len);
	vfs_write(0, buf, 0, &len);
	vfs_close(fd);

	return 0;
}

int test_thread2()
{
	while(1) {
		char str[65];
		size_t len = sizeof(str) - 1;
		
		if(vfs_read(0, str, 0, &len) < 0) {
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
		kprintf_xy(0, tid-1, "thread%d - %d    - %d %d - cpu %d", tid, a, system_time(), smp_get_current_cpu());
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
	struct thread *t;
	int i;
	char temp[64];
	
#if 1
	for(i=0; i<NUM_TEST_THREADS; i++) {
		sprintf(temp, "test_thread%d", i);
//		t = create_kernel_thread(NULL, temp, &test_thread, i % THREAD_NUM_PRIORITY_LEVELS);
		t = create_kernel_thread(NULL, temp, &test_thread, 5);
		thread_enqueue_run_q(t);
		if(i == 0) {
			thread_test_first_thid = t->id;
		}
		sprintf(temp, "test sem %d", i);
		thread_test_sems[i] = sem_create(0, temp);
	}	
	t = create_kernel_thread(NULL, "test starter thread", &test_thread_starter_thread, THREAD_MAX_PRIORITY);
	thread_enqueue_run_q(t);
#endif

	t = create_kernel_thread(NULL, "test thread 2", &test_thread2, 5);
	thread_enqueue_run_q(t);

	t = create_kernel_thread(NULL, "test thread 3", &test_thread3, 5);
	thread_enqueue_run_q(t);

	t = create_kernel_thread(NULL, "test thread 4", &test_thread4, 5);
	thread_enqueue_run_q(t);

//	t = create_kernel_thread(NULL, "test thread 5", &test_thread5, 5);
//	thread_enqueue_run_q(t);

//	t = create_kernel_thread(NULL, "panic thread", &panic_thread, THREAD_MAX_PRIORITY);
//	thread_enqueue_run_q(t);

	dprintf("thread_test: done creating test threads\n");
	
	return 0;
}

static int
_rand()
{
	static int next = 0;

	if(next == 0)
		next = system_time();

	next = next * 1103515245 + 12345;
	return((next >> 16) & 0x7FFF);
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
	timer_setup_timer(NULL, NULL, &LOCAL_CPU_TIMER);
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

static struct proc *create_proc_struct(const char *name, bool kernel)
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
	if(arch_proc_init_proc_struct(p, kernel) < 0) {
		// XXX clean up proc struct
		return NULL;
	}
	
	return p;
}

static int proc_create_proc2(void)
{
	int err;
	struct thread *t;
	struct proc *p;
	char *path;
	addr entry;

	dprintf("proc_create_proc2: entry\n");

	t = thread_get_current_thread();
	p = t->proc;

	p->aspace = vm_create_aspace(p->name, USER_BASE, USER_SIZE);
	if(p->aspace == NULL) {
		// XXX clean up proc
		return NULL;
	}
	
	path = t->args;
	dprintf("proc_create_proc2: loading elf binary '%s'\n", path);

	err = elf_load(path, p, 0, &entry);
	if(err < 0){
		// XXX clean up proc
		return NULL;
	}
	 
	dprintf("proc_create_proc2: loaded elf. entry = 0x%x\n", entry);

	return 0;
}

struct proc *proc_create_proc(const char *path, const char *name, int priority)
{
	struct proc *p;
	struct thread *t;
	int err;
	unsigned int state;
	
	dprintf("proc_create_proc: entry '%s', name '%s'\n", path, name);
	
	p = create_proc_struct(name, false);
	if(p == NULL)
		return NULL;

	t = create_kernel_thread(p, name, proc_create_proc2, priority);
	if(t == NULL) {
		// XXX clean up proc
		return NULL;
	}
	
	// copy the args over
	t->args = kmalloc(strlen(path) + 1);
	strcpy(t->args, path);
	
	state = int_disable_interrupts();
	GRAB_PROC_LOCK();
	hash_insert(proc_hash, p);
	RELEASE_PROC_LOCK();

	GRAB_THREAD_LOCK();
	thread_enqueue_run_q(t);
	RELEASE_THREAD_LOCK();
	int_restore_interrupts(state);

	return p;
}
