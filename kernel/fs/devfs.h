#ifndef _DEVFS_H
#define _DEVFS_H

#include "dev.h"

int bootstrap_devfs();
int devfs_create_device_node(const char *path, struct device_hooks *hooks);

#endif
