/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <kernel/user_runtime.h>
#include <sys/syscalls.h>

extern void sys_exit(int retcode);

extern void (*__ctor_list)(void);
extern void (*__ctor_end)(void);

int _start(unsigned image_id, struct uspace_prog_args_t *);
static void _call_ctors(void);

int
_start(unsigned image_id, struct uspace_prog_args_t *uspa)
{
	int i;
	int retcode;

	_call_ctors();

	return 0;
}

static
void _call_ctors(void)
{
	void (**f)(void);

	for(f = &__ctor_list; f < &__ctor_end; f++) {
		(**f)();
	}
}

int __gxx_personality_v0(void);

// XXX hack to make gcc 3.x happy until we get libstdc++ working
int __gxx_personality_v0(void)
{
	return 0;
}
