/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _STAGE2_PRIV_H
#define _STAGE2_PRIV_H

#define LOAD_ADDR 0x100000
#define BOOTDIR_ADDR 0x101000

int s2_text_init(kernel_args *ka);
int s2_get_text_line(void);
void s2_change_framebuffer_addr(kernel_args *ka, unsigned int address);
void putchar(char c);
void puts(char *str);
int printf(const char *fmt, ...);
void switch_stacks_and_call(unsigned int new_stack, void *func, int arg0, int arg1);

/* openfirmware calls */
int of_init(void *of_entry);
int of_open(const char *node_name);
int of_finddevice(const char *dev);
int of_instance_to_package(int in_handle);
int of_getprop(int handle, const char *prop, void *buf, int buf_len);
int of_setprop(int handle, const char *prop, const void *buf, int buf_len);
int of_read(int handle, void *buf, int buf_len);
int of_write(int handle, void *buf, int buf_len);
int of_seek(int handle, long long pos);
void of_exit(void);
void *of_claim(unsigned int vaddr, unsigned int size, unsigned int align);

int s2_mmu_init(kernel_args *ka);
int s2_mmu_remap_pagetable(kernel_args *ka);
void mmu_map_page(kernel_args *ka, unsigned long pa, unsigned long va);
unsigned long mmu_allocate_page(kernel_args *ka);
void syncicache(void *address, int len);

void s2_faults_init(kernel_args *ka);

void getibats(int bats[8]);
void setibats(int bats[8]);
void getdbats(int bats[8]);
void setdbats(int bats[8]);
unsigned int *getsdr1(void);
void setsdr1(unsigned int sdr);
unsigned int getsr(unsigned int va);
void setsr(unsigned int va, unsigned int val);
unsigned int getmsr(void);
void setmsr(unsigned int msr);

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
#define ROUNDOWN(a, b) (((a) / (b)) * (b))

#endif
