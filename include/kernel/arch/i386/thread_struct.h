#ifndef _I386_THREAD_STRUCT_H
#define _I386_THREAD_STRUCT_H

// architecture specific thread info
struct arch_thread {
	unsigned int *esp;
};

struct arch_proc {
	unsigned int *pgdir;
};

#endif

