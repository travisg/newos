#ifndef __SERIAL_H
#define __SERIAL_H

int init_serial(void);
void serial_write(const void *_buf, int len);
int serial_read(void *_buf, int len);

#endif
