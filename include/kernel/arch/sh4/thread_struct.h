#ifndef _ARCH_THREAD_STRUCT_H
#define _ARCH_THREAD_STRUCT_H

// architecture specific thread info
struct arch_thread {
	unsigned int *sp;
};

struct arch_proc {
	unsigned int *pgdir;
};

#endif

