#include "of.h"
#include "isa.h"

#define SERIAL_REG(x) (*(volatile unsigned char *)(0xfe000000 | (x)))

int init_serial(void)
{
    isa_write8(0x2fb, 0x80);
    isa_write8(0x2f8, 1);
    isa_write8(0x2f9, 0);
    isa_write8(0x2fb, 0x3);

    return 0;
}

void serial_write(const void *_buf, int len)
{
    const unsigned char *buf = _buf;
    int i;

    for (i=0; i < len; i++) {
        while ((isa_read8(0x2fd) & 0x20) == 0)
            ;
        isa_write8(0x2f8, buf[i]);
    }
}

int serial_read(void *_buf, int len)
{
    unsigned char *buf = _buf;
    int i;

    for (i=0; i < len; i++) {
        while ((isa_read8(0x2fd) & 1) == 0)
            ;
        buf[i] = isa_read8(0x2f8);
    }
    return i;
}

int serial_read_int32(int *buf)
{
    if (serial_read(buf, 4) != 4)
        return -1;
    return 0;
}
