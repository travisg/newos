#ifdef ARCH_i386
#include "arch/i386/syscalls.inc"
#endif
#ifdef ARCH_x86_64
#include "arch/x86_64/syscalls.inc"
#endif
#ifdef ARCH_alpha
#include "arch/alpha/syscalls.inc"
#endif
#ifdef ARCH_sh4
#include "arch/sh4/syscalls.inc"
#endif
#ifdef ARCH_sparc
#include "arch/sparc/syscalls.inc"
#endif
#ifdef ARCH_sparc64
#include "arch/sparc64/syscalls.inc"
#endif
#ifdef ARCH_mips
#include "arch/mips/syscalls.inc"
#endif
#ifdef ARCH_ppc
#include "arch/ppc/syscalls.inc"
#endif
#ifdef ARCH_m68k
#include "arch/m68k/syscalls.inc"
#endif
#ifdef ARCH_arm
#include "arch/arm/syscalls.inc"
#endif



SYSCALL0(_kern_null, 0)
SYSCALL4(_kern_mount, 1)
SYSCALL1(_kern_unmount, 2)
SYSCALL0(_kern_sync, 3)
SYSCALL2(_kern_open, 4)
SYSCALL1(_kern_close, 5)
SYSCALL1(_kern_fsync, 6)
SYSCALL5(_kern_read, 7)
SYSCALL5(_kern_write, 8)
SYSCALL4(_kern_seek, 9)
SYSCALL4(_kern_ioctl, 10)
SYSCALL1(_kern_create, 11)
SYSCALL1(_kern_unlink, 12)
SYSCALL2(_kern_rename, 13)
SYSCALL2(_kern_rstat, 14)
SYSCALL3(_kern_wstat, 15)
SYSCALL0(_kern_system_time, 16)
SYSCALL2(_kern_snooze, 17)
SYSCALL2(_kern_sem_create, 18)
SYSCALL1(_kern_sem_delete, 19)
SYSCALL2(_kern_sem_acquire, 20)
SYSCALL5(_kern_sem_acquire_etc, 21)
SYSCALL2(_kern_sem_release, 22)
SYSCALL3(_kern_sem_release_etc, 23)
SYSCALL0(_kern_get_current_thread_id, 24)
SYSCALL1(_kern_exit, 25)
SYSCALL6(_kern_proc_create_proc, 26)
SYSCALL2(_kern_thread_wait_on_thread, 27)
SYSCALL2(_kern_proc_wait_on_proc, 28)
SYSCALL6(_kern_vm_create_anonymous_region, 29)
SYSCALL6(_kern_vm_clone_region, 30)
SYSCALL9(_kern_vm_map_file, 31)
SYSCALL1(_kern_vm_delete_region, 32)
SYSCALL2(_kern_vm_get_region_info, 33)
SYSCALL3(_kern_thread_create_thread, 34)
SYSCALL1(_kern_thread_kill_thread, 35)
SYSCALL1(_kern_thread_suspend_thread, 36)
SYSCALL1(_kern_thread_resume_thread, 37)
SYSCALL1(_kern_proc_kill_proc, 38)
SYSCALL0(_kern_get_current_proc_id, 39)
SYSCALL2(_kern_getcwd, 40)
SYSCALL1(_kern_setcwd, 41)
SYSCALL2(_kern_port_create, 42)
SYSCALL1(_kern_port_close, 43)
SYSCALL1(_kern_port_delete, 44)
SYSCALL1(_kern_port_find, 45)
SYSCALL2(_kern_port_get_info, 46)
SYSCALL3(_kern_port_get_next_port_info, 47)
SYSCALL1(_kern_port_buffer_size, 48)
SYSCALL4(_kern_port_buffer_size_etc, 49)
SYSCALL1(_kern_port_count, 50)
SYSCALL4(_kern_port_read, 51)
SYSCALL7(_kern_port_read_etc, 52)
SYSCALL2(_kern_port_set_owner, 53)
SYSCALL4(_kern_port_write, 54)
SYSCALL7(_kern_port_write_etc, 55)
SYSCALL2(_kern_sem_get_count, 56)
SYSCALL2(_kern_sem_get_sem_info, 57)
SYSCALL3(_kern_sem_get_next_sem_info, 58)
SYSCALL2(_kern_set_sem_owner, 59)
SYSCALL1(_kern_dup, 60)
SYSCALL2(_kern_dup2, 61)
SYSCALL2(_kern_getrlimit, 62)
SYSCALL2(_kern_setrlimit, 63)
SYSCALL2(_kern_atomic_add, 64)
SYSCALL2(_kern_atomic_and, 65)
SYSCALL2(_kern_atomic_or, 66)
SYSCALL2(_kern_atomic_set, 67)
SYSCALL3(_kern_test_and_set, 68)
SYSCALL2(_kern_thread_get_thread_info, 69)
SYSCALL3(_kern_thread_get_next_thread_info, 70)
SYSCALL2(_kern_proc_get_proc_info, 71)
SYSCALL2(_kern_proc_get_next_proc_info, 72)
SYSCALL2(_kern_thread_set_priority, 73)
SYSCALL1(_kern_opendir, 74)
SYSCALL1(_kern_closedir, 75)
SYSCALL1(_kern_rewinddir, 76)
SYSCALL3(_kern_readdir, 77)
SYSCALL1(_kern_mkdir, 78)
SYSCALL1(_kern_rmdir, 79)
SYSCALL1(_kern_vm_get_vm_info, 80)
SYSCALL0(_kern_sigreturn, 81)
SYSCALL3(_kern_sigaction, 82)
SYSCALL2(_kern_send_signal, 83)
SYSCALL2(_kern_send_proc_signal, 84)
SYSCALL3(_kern_set_alarm, 85)
SYSCALL2(_kern_setpgid, 86)
SYSCALL1(_kern_getpgid, 87)
SYSCALL0(_kern_setsid, 88)
