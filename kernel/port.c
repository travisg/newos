/*
** Copyright 2001, Mark-Jan Bastian. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/port.h>
#include <kernel/kernel.h>
#include <kernel/sem.h>
#include <kernel/int.h>
#include <kernel/debug.h>
#include <kernel/heap.h>
#include <kernel/cbuf.h>
#include <sys/errors.h>

#include <libc/string.h>

struct port_msg {
	int		msg_code;
	cbuf*	data_cbuf;
	size_t	data_len;
};

struct port_entry {
	port_id 			id;
	proc_id 			owner;
	int32 				capacity;
	int     			lock;
	char				*name;
	sem_id				read_sem;
	sem_id				write_sem;
	int					head;
	int					tail;
	int					total_count;
	bool				closed;
	struct port_msg*	msg_queue;
};

// internal API
void dump_port_list(int argc, char **argv);
static void _dump_port_info(struct port_entry *port);
static void dump_port_info(int argc, char **argv);


// MAX_PORTS must be power of 2
#define MAX_PORTS 4096
#define MAX_QUEUE_LENGTH 4096
#define PORT_MAX_MESSAGE_SIZE 65536

static struct port_entry *ports = NULL;
static region_id port_region = 0;
static bool ports_active = false;

static port_id next_port = 0;

static int port_spinlock = 0;
#define GRAB_PORT_LIST_LOCK() acquire_spinlock(&port_spinlock)
#define RELEASE_PORT_LIST_LOCK() release_spinlock(&port_spinlock)
#define GRAB_PORT_LOCK(s) acquire_spinlock(&(s).lock)
#define RELEASE_PORT_LOCK(s) release_spinlock(&(s).lock)

int port_init(kernel_args *ka)
{
	int i;
	int sz;

	dprintf("port_init: entry\n");
	
	sz = sizeof(struct port_entry) * MAX_PORTS;

	// create and initialize semaphore table
	port_region = vm_create_anonymous_region(vm_get_kernel_aspace_id(), "port_table", (void **)&ports,
		REGION_ADDR_ANY_ADDRESS, sz, REGION_WIRING_WIRED, LOCK_RW|LOCK_KERNEL);
	if(port_region < 0) {
		panic("unable to allocate kernel port table!\n");
	}

	memset(ports, 0, sz);
	for(i=0; i<MAX_PORTS; i++)
		ports[i].id = -1;

	// add debugger commands
	dbg_add_command(&dump_port_list, "ports", "Dump a list of all active ports");
	dbg_add_command(&dump_port_info, "port", "Dump info about a particular port");

	ports_active = true;

	dprintf("port_init: exit, next_port = %d\n", next_port);

	return 0;
}

void dump_port_list(int argc, char **argv)
{
	int i;

	for(i=0; i<MAX_PORTS; i++) {
		if(ports[i].id >= 0) {
			dprintf("0x%x\tid: 0x%x\t\tname: '%s'\n", &ports[i], ports[i].id, ports[i].name);
		}
	}
}

static void _dump_port_info(struct port_entry *port)
{
	int cnt;
	dprintf("PORT:   0x%x\n", port);
	dprintf("name:  '%s'\n", port->name);
	dprintf("owner: 0x%x\n", port->owner);
	dprintf("cap:  %d\n", port->capacity);
	dprintf("head: %d\n", port->head);
	dprintf("tail: %d\n", port->tail);
 	sem_get_count(port->read_sem, &cnt);
 	dprintf("read_sem:  %d\n", cnt);
 	sem_get_count(port->read_sem, &cnt);
	dprintf("write_sem: %d\n", cnt);
}

static void dump_port_info(int argc, char **argv)
{
	int i;

	if(argc < 2) {
		dprintf("port: not enough arguments\n");
		return;
	}

	// if the argument looks like a hex number, treat it as such
	if(strlen(argv[1]) > 2 && argv[1][0] == '0' && argv[1][1] == 'x') {
		unsigned long num = atoul(argv[1]);

		if(num > KERNEL_BASE && num <= (KERNEL_BASE + (KERNEL_SIZE - 1))) {
			// XXX semi-hack
			// one can use either address or a port_id, since KERNEL_BASE > MAX_PORTS assumed
			_dump_port_info((struct port_entry *)num);
			return;
		} else {
			int slot = num % MAX_PORTS;
			if(ports[slot].id != (int)num) {
				dprintf("port 0x%x doesn't exist!\n", num);
				return;
			}
			_dump_port_info(&ports[slot]);
			return;
		}
	}

	// walk through the ports list, trying to match name
	for(i=0; i<MAX_PORTS; i++) {
		if(strcmp(argv[1], ports[i].name) == 0) {
			_dump_port_info(&ports[i]);
			return;
		}
	}
}

port_id		
port_create(int32 queue_length, const char *name)
{
	int 	i;
	int 	state;
	sem_id 	sem_r, sem_w;
	port_id retval;
	char 	*temp_name;
	void 	*q;
	proc_id	owner;
	
	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;

	// check & dup name
	if(name) {
		int name_len = strlen(name);

		temp_name = (char *)kmalloc(min(name_len + 1, SYS_MAX_OS_NAME_LEN));
		if(temp_name == NULL)
			return ERR_NO_MEMORY;
		strncpy(temp_name, name, SYS_MAX_OS_NAME_LEN-1);
		temp_name[SYS_MAX_OS_NAME_LEN-1] = 0;
	} else {
		temp_name = (char *)kmalloc(sizeof("default_port_name")+1);
		if(temp_name == NULL)
			return ERR_NO_MEMORY;
		strcpy(temp_name, "default_port_name");
	}
	
	// check queue length & alloc
	if (queue_length < 1)
		return ERR_INVALID_ARGS;
	if (queue_length > MAX_QUEUE_LENGTH)
		return ERR_INVALID_ARGS;
	q = kmalloc( queue_length * sizeof(struct port_msg) );
	if (q == NULL) {
		kfree(temp_name); // dealloc name, too
		return ERR_NO_MEMORY;
	}

	// create sem_r with owner set to -1
	sem_r = sem_create_etc(0, temp_name, -1);
	if (sem_r < 0) {
		// cleanup
		kfree(temp_name);
		kfree(q);
		return sem_r;
	}

	// create sem_w
	sem_w = sem_create_etc(queue_length, temp_name, -1);
	if (sem_w < 0) {
		// cleanup
		sem_delete(sem_r);
		kfree(temp_name);
		kfree(q);
		return sem_w;
	}
	owner = proc_get_kernel_proc_id();

	state = int_disable_interrupts();
	GRAB_PORT_LIST_LOCK();

	// find the first empty spot
	for(i=0; i<MAX_PORTS; i++) {
		if(ports[i].id == -1) {
			// make the port_id be a multiple of the slot it's in
			if(i >= next_port % MAX_PORTS) {
				next_port += i - next_port % MAX_PORTS;
			} else {
				next_port += MAX_PORTS - (next_port % MAX_PORTS - i);
			}
			ports[i].id		= next_port++;
			ports[i].lock	= 0;
			GRAB_PORT_LOCK(ports[i]);
			RELEASE_PORT_LIST_LOCK();

			ports[i].capacity	= queue_length;
			ports[i].name 		= temp_name;

			// assign sem
			ports[i].read_sem	= sem_r;
			ports[i].write_sem	= sem_w;
			ports[i].msg_queue	= q;
			ports[i].head 		= 0;
			ports[i].tail 		= 0;
			ports[i].total_count= 0;
			ports[i].owner 		= owner;
			retval = ports[i].id;
			RELEASE_PORT_LOCK(ports[i]);
			goto out;
		}
	}
	// not enough ports...
	RELEASE_PORT_LIST_LOCK();
	kfree(q);
	kfree(temp_name);
	retval = ERR_PORT_OUT_OF_SLOTS;
	dprintf("port_create(): ERR_PORT_OUT_OF_SLOTS\n");

	// cleanup
	sem_delete(sem_w);
	sem_delete(sem_r);
	kfree(temp_name);
	kfree(q);

out:
	int_restore_interrupts(state);
	dprintf("port_create(): port_id %d done, sem r %d w %d\n", retval, sem_r, sem_w);

	return retval;
}

int			
port_close(port_id id)
{
	int 	state;
	int		slot;

	dprintf("port_close(): port_id %d\n", id);

	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;
	slot = id % MAX_PORTS;

	// walk through the sem list, trying to match name
	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	if (ports[slot].id != id) {
		RELEASE_PORT_LOCK(ports[slot]);
		int_restore_interrupts(state);
		return ERR_INVALID_HANDLE;
	}

	// mark port to disable writing
	ports[slot].closed = true;

	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);

	return NO_ERROR;
}

int
port_delete(port_id id)
{
	int slot;
	int state;
	sem_id	r_sem, w_sem;

	char *old_name;
	void *q;

	dprintf("port_delete(): port_id %d\n", id);

	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;

	slot = id % MAX_PORTS;

	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	if(ports[slot].id != id) {
		RELEASE_PORT_LOCK(ports[slot]);
		int_restore_interrupts(state);
		dprintf("port_delete: invalid port_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}

	/* mark port as invalid */
	ports[slot].id	 = -1;
	old_name 		 = ports[slot].name;
	q				 = ports[slot].msg_queue;
	r_sem			 = ports[slot].read_sem;
	w_sem			 = ports[slot].write_sem;
	ports[slot].name = NULL;

	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);

	kfree(q);
	kfree(old_name);

	// release the threads that were blocking on this port by deleting the sem
	// read_port() will see the ERR_SEM_DELETED acq_sem() return value, and act accordingly
	sem_delete(r_sem);
	sem_delete(w_sem);

	return NO_ERROR;
}

port_id
port_find(const char *port_name)
{
	int i;
	int state;
	int ret_val = ERR_INVALID_HANDLE;

	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if(port_name == NULL)
		return ERR_INVALID_HANDLE;

	// lock list of ports
	state = int_disable_interrupts();
	GRAB_PORT_LIST_LOCK();
	
	// loop over list
	for(i=0; i<MAX_PORTS; i++) {
		// lock every individual port before comparing
		GRAB_PORT_LOCK(ports[i]);
		if(strcmp(port_name, ports[i].name) == 0) {
			ret_val = ports[i].id;
			RELEASE_PORT_LOCK(ports[i]);
			break;
		}
		RELEASE_PORT_LOCK(ports[i]);
	}
	
	RELEASE_PORT_LIST_LOCK();
	int_restore_interrupts(state);

	return ret_val;
}

int
port_get_info(port_id id, struct port_info *info)
{
	int slot;
	int state;

	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if (info == NULL)
		return ERR_INVALID_ARGS;
	if(id < 0)
		return ERR_INVALID_HANDLE;

	slot = id % MAX_PORTS;

	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	if(ports[slot].id != id) {
		RELEASE_PORT_LOCK(ports[slot]);
		int_restore_interrupts(state);
		dprintf("port_get_info: invalid port_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}

	// fill a port_info struct with info
	info->id			= ports[slot].id;
	info->owner 		= ports[slot].owner;
	strncpy(info->name, ports[slot].name, min(strlen(ports[slot].name),SYS_MAX_OS_NAME_LEN-1));
	info->capacity		= ports[slot].capacity;
	sem_get_count(ports[slot].read_sem, &info->queue_count);
	info->total_count	= ports[slot].total_count;

	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);
	
	// from our port_entry
	return NO_ERROR;
}

int
port_get_next_port_info(proc_id proc,
						uint32 *cookie,
						struct port_info *info)
{
	int state;
	int slot;
	
	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if (cookie == NULL)
		return ERR_INVALID_ARGS;

	if (*cookie == NULL) {
		// return first found
		slot = 0;
	} else {
		// start at index cookie, but check cookie against MAX_PORTS
		slot = *cookie;
		if (slot >= MAX_PORTS)
			return ERR_INVALID_HANDLE;
	}

	// spinlock
	state = int_disable_interrupts();
	GRAB_PORT_LIST_LOCK();
	
	info->id = -1; // used as found flag
	while (slot < MAX_PORTS) {
		GRAB_PORT_LOCK(ports[slot]);
		if (ports[slot].id != -1)
			if (ports[slot].owner == proc) {
				// found one!
				// copy the info
				info->id			= ports[slot].id;
				info->owner 		= ports[slot].owner;
				strncpy(info->name, ports[slot].name, min(strlen(ports[slot].name),SYS_MAX_OS_NAME_LEN-1));
				info->capacity		= ports[slot].capacity;
				sem_get_count(ports[slot].read_sem, &info->queue_count);
				info->total_count	= ports[slot].total_count;
				RELEASE_PORT_LOCK(ports[slot]);
				slot++;
				break;
			}
		RELEASE_PORT_LOCK(ports[slot]);
		slot++;
	}
	RELEASE_PORT_LIST_LOCK();
	int_restore_interrupts(state);

	if (info->id == -1)
		return ERR_PORT_NOT_FOUND;
	*cookie = slot;
	return NO_ERROR;
}

ssize_t
port_buffer_size(port_id id)
{
	return port_buffer_size_etc(id, 0, 0);
}

ssize_t
port_buffer_size_etc(port_id id,
					uint32 flags,
					time_t timeout)
{
	int slot;
	int res;
	int t;
	int len;
	int state;
		
	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;

	slot = id % MAX_PORTS;

	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	if(ports[slot].id != id) {
		RELEASE_PORT_LOCK(ports[slot]);
		int_restore_interrupts(state);
		dprintf("port_get_info: invalid port_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}
	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);

	// block if no message, 
	// if TIMEOUT flag set, block with timeout

	// XXX - is it a race condition to acquire a sem just after we
	// unlocked the port ?
	// XXX: call an acquire_sem which does the release lock, restore int & block the right way
	res = sem_acquire_etc(ports[slot].read_sem, 1, flags & (SEM_FLAG_TIMEOUT | SEM_FLAG_INTERRUPTABLE), timeout, NULL);

	GRAB_PORT_LOCK(ports[slot]);
	if (res == ERR_SEM_DELETED) {
		// somebody deleted the port
		RELEASE_PORT_LOCK(ports[slot]);
		return ERR_PORT_DELETED;
	}
	if (res == ERR_SEM_TIMED_OUT) {
		RELEASE_PORT_LOCK(ports[slot]);
		return ERR_PORT_TIMED_OUT;
	}

	// once message arrived, read data's length
	
	// determine tail
	// read data's head length
	t = ports[slot].head;
	if (t < 0)
		panic("port %id: tail < 0", ports[slot].id);
	if (t > ports[slot].capacity)
		panic("port %id: tail > cap %d", ports[slot].id, ports[slot].capacity);
	len = ports[slot].msg_queue[t].data_len;
	
	// restore readsem
	sem_release(ports[slot].read_sem, 1);

	RELEASE_PORT_LOCK(ports[slot]);
	
	// return length of item at end of queue
	return len;
}

int32
port_count(port_id id)
{
	int slot;
	int state;
	int count;
	
	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;

	slot = id % MAX_PORTS;

	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	if(ports[slot].id != id) {
		RELEASE_PORT_LOCK(ports[slot]);
		int_restore_interrupts(state);
		dprintf("port_count: invalid port_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}
	
	sem_get_count(ports[slot].read_sem, &count);
	// do not return negative numbers 
	if (count < 0)
		count = 0;

	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);

	// return count of messages (sem_count)
	return count;
}

ssize_t
port_read(port_id port,
			int32 *msg_code,
			void *msg_buffer,
			size_t buffer_size)
{
	return port_read_etc(port, msg_code, msg_buffer, buffer_size, 0, 0);
}

ssize_t
port_read_etc(port_id id,
				int32	*msg_code,
				void	*msg_buffer,
				size_t	buffer_size,
				uint32	flags,
				time_t	timeout)
{
	int		slot;
	int 	state;
	sem_id	cached_semid;
	size_t 	siz;
	int		res;
	int		t;
	cbuf*	msg_store;
	int32	code;
	int		err;
	
	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;
	if(msg_code == NULL)
		return ERR_INVALID_ARGS;
	if((msg_buffer == NULL) && (buffer_size > 0))
		return ERR_INVALID_ARGS;
	if (timeout < 0)
		return ERR_INVALID_ARGS;

	slot = id % MAX_PORTS;

	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	if(ports[slot].id != id) {
		RELEASE_PORT_LOCK(ports[slot]);
		int_restore_interrupts(state);
		dprintf("read_port_etc: invalid port_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}
	// store sem_id in local variable
	cached_semid = ports[slot].read_sem;

	// unlock port && enable ints/
	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);

	dprintf("port_read_etc(): port_id %d, wait\n", id);
	
	// XXX -> possible race condition if port gets deleted (->sem deleted too), therefore
	// sem_id is cached in local variable up here
	
	// get 1 entry from the queue, block if needed
	res = sem_acquire_etc(cached_semid, 1,
						flags & (SEM_FLAG_TIMEOUT | SEM_FLAG_INTERRUPTABLE), timeout, NULL);

	dprintf("port_read_etc(): port_id %d, done %d\n", id, res);

	// XXX: possible race condition if port read by two threads...
	//      both threads will read in 2 different slots allocated above, simultaneously
	// 		slot is a thread-local variable

	if (res == ERR_SEM_DELETED) {
		// somebody deleted the port
		return ERR_PORT_DELETED;
	}

	if (res == ERR_SEM_INTERRUPTED) {
		// XXX: somebody signaled the process the port belonged to, deleting the sem ?
		return ERR_PORT_INTERRUPTED;
	}

	if (res == ERR_SEM_TIMED_OUT) {
		// timed out, or, if timeout=0, 'would block'
		return ERR_PORT_TIMED_OUT;
	}

	if (res != NO_ERROR) {
		dprintf("write_port_etc: res unknown error %d\n", res);
		return res;
	}

	// attach copied message to queue
	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	t = ports[slot].tail;
	if (t < 0)
		panic("port %id: tail < 0", ports[slot].id);
	if (t > ports[slot].capacity)
		panic("port %id: tail > cap %d", ports[slot].id, ports[slot].capacity);

	ports[slot].tail = (ports[slot].tail + 1) % ports[slot].capacity;

	msg_store	= ports[slot].msg_queue[t].data_cbuf;
	code 		= ports[slot].msg_queue[t].msg_code;

	// check output buffer size
	siz	= min(buffer_size, ports[slot].msg_queue[t].data_len);
	
	cached_semid = ports[slot].write_sem;

	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);

	// copy message
/*	if (siz > 0) {
		if (flags & PORT_FLAG_USE_USER_MEMCPY)
			user_memcpy(msg_buffer, buf, siz);
		else
			memcpy(msg_buffer, buf, siz);
	}
*/
	*msg_code = code;
	if (siz > 0) {
		if (flags & PORT_FLAG_USE_USER_MEMCPY) {
			if ((err = cbuf_user_memcpy_from_chain(msg_buffer, msg_store, 0, siz) < 0))	{
				sem_release(cached_semid, 1); // it's read anyway...
				return err;
			}
		} else
			cbuf_memcpy_from_chain(msg_buffer, msg_store, 0, siz);
	}

	// make one spot in queue available again for write
	sem_release(cached_semid, 1);

	return siz;
}

int
port_set_owner(port_id id, proc_id proc)
{
	int slot;
	int state;
	
	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;

	slot = id % MAX_PORTS;

	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	if(ports[slot].id != id) {
		RELEASE_PORT_LOCK(ports[slot]);
		int_restore_interrupts(state);
		dprintf("port_set_owner: invalid port_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}

	// transfer ownership to other process
	ports[slot].owner = proc;

	// unlock port
	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);

	return NO_ERROR;
}

int
port_write(port_id id,
	int32 msg_code,
	void *msg_buffer,
	size_t buffer_size)
{
	return port_write_etc(id, msg_code, msg_buffer, buffer_size, 0, 0);
}

int
port_write_etc(port_id id,
	int32 msg_code,
	void *msg_buffer,
	size_t buffer_size,
	uint32 flags,
	time_t timeout)
{
	int slot;
	int state;
	int res;
	sem_id cached_semid;
	int h;
	cbuf* msg_store;
	int c1, c2;
	int err;
	
	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;
	if(id < 0)
		return ERR_INVALID_HANDLE;

	slot = id % MAX_PORTS;
	
	// check buffer_size
	if (buffer_size > PORT_MAX_MESSAGE_SIZE)
		return ERR_INVALID_ARGS;

	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	if(ports[slot].id != id) {
		RELEASE_PORT_LOCK(ports[slot]);
		int_restore_interrupts(state);
		dprintf("write_port_etc: invalid port_id %d\n", id);
		return ERR_INVALID_HANDLE;
	}

	if (ports[slot].closed) {
		RELEASE_PORT_LOCK(ports[slot]);
		int_restore_interrupts(state);
		dprintf("write_port_etc: port %d closed\n", id);
		return ERR_PORT_CLOSED;
	}
	
	// store sem_id in local variable 
	cached_semid = ports[slot].write_sem;

	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);
	
	dprintf("port_write_etc(): port_id %d, wait\n", id);
	
	// XXX -> possible race condition if port gets deleted (->sem deleted too), 
	// and queue is full therefore sem_id is cached in local variable up here
	
	// get 1 entry from the queue, block if needed
	res = sem_acquire_etc(cached_semid, 1,
						flags & (SEM_FLAG_TIMEOUT | SEM_FLAG_INTERRUPTABLE), timeout, NULL);

	dprintf("port_write_etc(): port_id %d, done %d\n", id, res);

	// XXX: possible race condition if port written by two threads...
	//      both threads will write in 2 different slots allocated above, simultaneously
	// 		slot is a thread-local variable

	if (res == ERR_SEM_DELETED) {
		// somebody deleted the port
		return ERR_PORT_DELETED;
	}

	if (res == ERR_SEM_INTERRUPTED) {
		// XXX: somebody signaled the process the port belonged to, deleting the sem ?
		return ERR_PORT_INTERRUPTED;
	}

	if (res == ERR_SEM_TIMED_OUT) {
		// timed out, or, if timeout=0, 'would block'
		return ERR_PORT_TIMED_OUT;
	}

	if (res != NO_ERROR) {
		dprintf("write_port_etc: res unknown error %d\n", res);
		return res;
	}

	msg_store = cbuf_get_chain(buffer_size);
	if (msg_store == NULL)
		return ERR_NO_MEMORY;
	if (flags & PORT_FLAG_USE_USER_MEMCPY)
		// copy from user memory
		if ((err = cbuf_user_memcpy_to_chain(msg_store, 0, msg_buffer, buffer_size)) < 0)
			return err; // memory exception
	else
		// copy from kernel memory
		if ((err = cbuf_memcpy_to_chain(msg_store, 0, msg_buffer, buffer_size)) < 0)
			return err; // memory exception

	// attach copied message to queue
	state = int_disable_interrupts();
	GRAB_PORT_LOCK(ports[slot]);

	h = ports[slot].head;
	if (h < 0)
		panic("port %id: head < 0", ports[slot].id);
	if (h >= ports[slot].capacity)
		panic("port %id: head > cap %d", ports[slot].id, ports[slot].capacity);
	ports[slot].msg_queue[h].msg_code	= msg_code;
	ports[slot].msg_queue[h].data_cbuf	= msg_store;
	ports[slot].msg_queue[h].data_len	= buffer_size;
	ports[slot].head = (ports[slot].head + 1) % ports[slot].capacity;
	ports[slot].total_count++;

	// store sem_id in local variable 
	cached_semid = ports[slot].read_sem;

	RELEASE_PORT_LOCK(ports[slot]);
	int_restore_interrupts(state);

	sem_get_count(ports[slot].read_sem, &c1);
	sem_get_count(ports[slot].write_sem, &c2);
	dprintf("port_write_etc(): meta complete, semcount read_sem %d write_sem %d\n", c1, c2);
	dprintf("port_write_etc(): will release sem %x now\n", cached_semid);

	// release sem, allowing read (might reschedule)
	sem_release(cached_semid, 1);

	return NO_ERROR;
}

/* this function cycles through the ports table, deleting all the ports that are owned by
   the passed proc_id */
int port_delete_owned_ports(proc_id owner)
{
	int state;
	int i;
	int count = 0;
	
	dprintf("port_delete_owned_ports(%d) enter\n", owner);

	if(ports_active == false)
		return ERR_PORT_NOT_ACTIVE;

	state = int_disable_interrupts();
	GRAB_PORT_LIST_LOCK();

	for(i=0; i<MAX_PORTS; i++) {
		if(ports[i].id != -1 && ports[i].owner == owner) {
			port_id id = ports[i].id;

			RELEASE_PORT_LIST_LOCK();
			int_restore_interrupts(state);

			port_delete(id);
			count++;

			state = int_disable_interrupts();
			GRAB_PORT_LIST_LOCK();
		}
	}

	RELEASE_PORT_LIST_LOCK();
	int_restore_interrupts(state);

	dprintf("port_delete_owned_ports(%d) exit\n", owner);

	return count;
}


/*
 * testcode
 */

port_id test_p1, test_p2, test_p3;

void port_test()
{
	char testdata[5];
	thread_id t;
	int res;

	strcpy(testdata, "abcd");

	dprintf("porttest: port_create()\n");
	test_p1 = port_create(1,    "test port #1");
	test_p2 = port_create(10,   "test port #2");
	test_p3 = port_create(1024, "test port #3");

	dprintf("porttest: port_find()\n");
	dprintf("'test port #1' has id %d (should be %d)\n", port_find("test port #1"), test_p1);

	dprintf("porttest: port_write() on 1, 2 and 3\n");
	port_write(test_p1, 1, &testdata, sizeof(testdata));
	port_write(test_p2, 666, &testdata, sizeof(testdata));
	port_write(test_p3, 999, &testdata, sizeof(testdata));
	dprintf("porttest: port_count(test_p1) = %d\n", port_count(test_p1));

	dprintf("porttest: spawning thread for port 1\n");
	t = thread_create_kernel_thread("port_test", port_test_thread_func, NULL);
	// resume thread
	thread_resume_thread(t);
	
	dprintf("porttest: write\n");
	port_write(test_p1, 1, &testdata, sizeof(testdata));

	// now we can write more (no blocking)
	dprintf("porttest: write #2\n");
	port_write(test_p1, 2, &testdata, sizeof(testdata));
	dprintf("porttest: write #3\n");
	port_write(test_p1, 3, &testdata, sizeof(testdata));

	dprintf("porttest: waiting on spawned thread\n");
	thread_wait_on_thread(t, NULL);

	dprintf("porttest: close p1\n");
	port_close(test_p2);
	dprintf("porttest: attempt write p1 after close\n");
	res = port_write(test_p2, 4, &testdata, sizeof(testdata));
	dprintf("porttest: port_write ret %d\n", res);

	dprintf("porttest: testing delete p2\n");
	port_delete(test_p2);

	dprintf("porttest: end test main thread\n");
	
}

int port_test_thread_func(void* arg)
{
	int msg_code;
	int n;
	char buf[5];

	dprintf("porttest: port_test_thread_func()\n");
	
	n = port_read(test_p1, &msg_code, &buf, 3);
	dprintf("port_read #1 code %d len %d buf %3s\n", msg_code, n, buf);
	n = port_read(test_p1, &msg_code, &buf, 4);
	dprintf("port_read #1 code %d len %d buf %4s\n", msg_code, n, buf);
	buf[4] = 'X';
	n = port_read(test_p1, &msg_code, &buf, 5);
	dprintf("port_read #1 code %d len %d buf %5s\n", msg_code, n, buf);

	dprintf("porttest: testing delete p1 from other thread\n");
	port_delete(test_p1);
	dprintf("porttest: end port_test_thread_func()\n");
	
	return 0;
}

/* 
 *	user level ports
 */

port_id user_port_create(int32 queue_length, const char *uname)
{
	if(uname != NULL) {
		char name[SYS_MAX_OS_NAME_LEN];
		int rc;

		if((addr)uname >= KERNEL_BASE && (addr)uname <= KERNEL_TOP)
			return ERR_VM_BAD_USER_MEMORY;

		rc = user_strncpy(name, uname, SYS_MAX_OS_NAME_LEN-1);
		if(rc < 0)
			return rc;
		name[SYS_MAX_OS_NAME_LEN-1] = 0;

		return port_create(queue_length, name);
	} else {
		return port_create(queue_length, NULL);
	}
}

int	user_port_close(port_id id)
{
	return port_close(id);
}

int	user_port_delete(port_id id)
{
	return port_delete(id);
}

port_id	user_port_find(const char *port_name)
{
	if(port_name != NULL) {
		char name[SYS_MAX_OS_NAME_LEN];
		int rc;

		if((addr)port_name >= KERNEL_BASE && (addr)port_name <= KERNEL_TOP)
			return ERR_VM_BAD_USER_MEMORY;

		rc = user_strncpy(name, port_name, SYS_MAX_OS_NAME_LEN-1);
		if(rc < 0)
			return rc;
		name[SYS_MAX_OS_NAME_LEN-1] = 0;

		return port_find(name);
	} else {
		return ERR_INVALID_ARGS;
	}
}

int	user_port_get_info(port_id id, struct port_info *uinfo)
{
	int 				res;
	struct port_info	info;
	int					rc;

	if (uinfo == NULL)
		return ERR_INVALID_ARGS;
	if((addr)uinfo >= KERNEL_BASE && (addr)uinfo <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	res = port_get_info(id, &info);
	// copy to userspace
	rc = user_memcpy(uinfo, &info, sizeof(struct port_info));
	if(rc < 0)
		return rc;
	return res;
}

int	user_port_get_next_port_info(proc_id uproc,
				uint32 *ucookie,
				struct port_info *uinfo)
{
	int 				res;
	struct port_info	info;
	uint32				cookie;
	int					rc;

	if (ucookie == NULL)
		return ERR_INVALID_ARGS;
	if (uinfo == NULL)
		return ERR_INVALID_ARGS;
	if((addr)ucookie >= KERNEL_BASE && (addr)ucookie <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;
	if((addr)uinfo >= KERNEL_BASE && (addr)uinfo <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	// copy from userspace
	rc = user_memcpy(&cookie, ucookie, sizeof(uint32));
	if(rc < 0)
		return rc;
	
	res = port_get_next_port_info(uproc, &cookie, &info);
	// copy to userspace
	rc = user_memcpy(ucookie, &info, sizeof(uint32));
	if(rc < 0)
		return rc;
	rc = user_memcpy(uinfo,   &info, sizeof(struct port_info));
	if(rc < 0)
		return rc;
	return res;
}

ssize_t	user_port_buffer_size_etc(port_id port, uint32 flags, time_t timeout)
{
	return port_buffer_size_etc(port, flags | SEM_FLAG_INTERRUPTABLE, timeout);
}

int32 user_port_count(port_id port)
{
	return port_count(port);
}

ssize_t	user_port_read_etc(port_id uport, int32 *umsg_code, void *umsg_buffer,
							size_t ubuffer_size, uint32 uflags, time_t utimeout)
{
	ssize_t	res;
	int32	msg_code;
	int		rc;
	
	if (umsg_code == NULL)
		return ERR_INVALID_ARGS;
	if (umsg_buffer == NULL)
		return ERR_INVALID_ARGS;

	if((addr)umsg_code >= KERNEL_BASE && (addr)umsg_code <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;
	if((addr)umsg_buffer >= KERNEL_BASE && (addr)umsg_buffer <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;

	res = port_read_etc(uport, &msg_code, umsg_buffer, ubuffer_size, 
						uflags | PORT_FLAG_USE_USER_MEMCPY | SEM_FLAG_INTERRUPTABLE, utimeout);

	rc = user_memcpy(umsg_code, &msg_code, sizeof(int32));
	if(rc < 0)
		return rc;

	return res;
}

int	user_port_set_owner(port_id port, proc_id proc)
{
	return port_set_owner(port, proc);
}

int	user_port_write_etc(port_id uport, int32 umsg_code, void *umsg_buffer,
				size_t ubuffer_size, uint32 uflags, time_t utimeout)
{
	if (umsg_buffer == NULL)
		return ERR_INVALID_ARGS;
	if((addr)umsg_buffer >= KERNEL_BASE && (addr)umsg_buffer <= KERNEL_TOP)
		return ERR_VM_BAD_USER_MEMORY;
	return port_write_etc(uport, umsg_code, umsg_buffer, ubuffer_size, 
		uflags | PORT_FLAG_USE_USER_MEMCPY | SEM_FLAG_INTERRUPTABLE, utimeout);
}

