#ifndef _ALPHA_THREAD_STRUCT_H
#define _ALPHA_THREAD_STRUCT_H

// architecture specific thread info
struct arch_thread {
	unsigned int *sp;
};

struct arch_proc {
	int foo;
};

#endif

