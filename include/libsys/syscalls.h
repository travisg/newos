#ifndef _LIBSYS_SYSCALLS_H
#define _LIBSYS_SYSCALLS_H

#include <types.h>
#include <kernel/ktypes.h>

typedef enum {
	STREAM_TYPE_NULL = 0,
	STREAM_TYPE_FILE,
	STREAM_TYPE_DIR,
	STREAM_TYPE_DEVICE,
} stream_type;

typedef enum {
	SEEK_SET = 0,
	SEEK_CUR,
	SEEK_END
} seek_type;

int sys_null();
int sys_open(const char *path, const char *stream, stream_type stream_type);
int sys_seek(int fd, off_t pos, seek_type seek_type);
int sys_read(int fd, void *buf, off_t pos, size_t *len);
int sys_write(int fd, const void *buf, off_t pos, size_t *len);
int sys_ioctl(int fd, int op, void *buf, size_t len);
int sys_close(int fd);
int sys_create(const char *path, const char *stream, stream_type stream_type);
time_t sys_system_time();
int sys_snooze(time_t time);

#endif