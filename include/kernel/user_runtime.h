/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef __newos__kernel__user_runtime__hh__
#define __newos__kernel__user_runtime__hh__


#include <sys/defines.h>


struct rld_export_t
{
	int (*dl_open )(char const *path);
	int (*dl_close)(int lib);
	int (*dl_sym  )(int lib, char const *sym);

	int (*load_addon)(char const *path);
};

struct uspace_prog_args_t
{
	char prog_name[SYS_MAX_OS_NAME_LEN];
	char prog_path[SYS_MAX_PATH_LEN];
	int  argc;
	int  envc;
	char **argv;
	char **envp;

	/*
	 * hooks into rld for POSIX and BeOS library/module loading
	 */
	struct rld_export_t *rld_export;
};


#endif
