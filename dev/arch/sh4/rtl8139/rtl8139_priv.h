#ifndef _RTL8139_PRIV_H
#define _RTL8139_PRIV_H

int rtl8139_detect();
int rtl8139_init();
void rtl8139_xmit(const char *ptr, int len);
int rtl8139_rx(char *buf, int buf_len);

#endif
