#ifndef __OF_H
#define __OF_H

/* openfirmware calls */
int of_init(void *of_entry);
int of_open(const char *node_name);
int of_open_args(const char *node_name, const char *args);
int of_finddevice(const char *dev);
int of_instance_to_package(int in_handle);
int of_getprop(int handle, const char *prop, void *buf, int buf_len);
int of_setprop(int handle, const char *prop, const void *buf, int buf_len);
int of_read(int handle, void *buf, int buf_len);
int of_write(int handle, void *buf, int buf_len);
int of_seek(int handle, long long pos);
void of_exit(void);
void *of_claim(unsigned int vaddr, unsigned int size, unsigned int align);

#endif
