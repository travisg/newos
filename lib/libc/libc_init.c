/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <sys/user_runtime.h>


void
INIT_BEFORE_CTORS(unsigned imid, struct uspace_prog_args_t const *uspa)
{
    _init__dlfcn(uspa);
}

