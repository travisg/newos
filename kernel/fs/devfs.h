#ifndef _DEVFS_H
#define _DEVFS_H

struct device_calls {
	int a;
};	

int bootstrap_devfs();
int devfs_create_device_node(const char *path, struct device_calls *calls);





#endif
