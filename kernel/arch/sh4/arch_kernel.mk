# sh4 kernel makefile
# included from kernel.mk
KERNEL_OBJS += \
        $(KERNEL_ARCH_DIR)/arch_asm.o \
        $(KERNEL_ARCH_DIR)/arch_cpu.o \
        $(KERNEL_ARCH_DIR)/arch_dbg_console.o \
        $(KERNEL_ARCH_DIR)/arch_debug.o \
        $(KERNEL_ARCH_DIR)/arch_faults.o \
        $(KERNEL_ARCH_DIR)/arch_int.o \
        $(KERNEL_ARCH_DIR)/arch_pmap.o \
        $(KERNEL_ARCH_DIR)/arch_timer.o \
        $(KERNEL_ARCH_DIR)/arch_thread.o \
        $(KERNEL_ARCH_DIR)/arch_vm.o
