/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#ifndef __newos__run_time_linker__hh__
#define __newos__run_time_linker__hh__


#include <types.h>


#define NEWOS_MAGIC_APPNAME	"__NEWOS_APP__"

typedef unsigned dynmodule_id;
int rldmain(void *arg);

dynmodule_id load_program(char const *path, void **entry);
dynmodule_id load_library(char const *path);
dynmodule_id load_addon(char const *path);
dynmodule_id unload_program(char const *path, void **entry);
dynmodule_id unload_library(char const *path, void **entry);
dynmodule_id unload_addon(char const *path, void **entry);

void  rldheap_init(void);
void *rldalloc(size_t);
void  rldfree(void *p);

int   export_dl_open(char const *, unsigned);
int   export_dl_close(int, unsigned);
void *export_dl_sym(int, char const *, unsigned);

int   export_load_addon(char const *, unsigned);
int   export_unload_addon(int, unsigned);
void *export_addon_symbol(int, char const *, unsigned);

#endif

