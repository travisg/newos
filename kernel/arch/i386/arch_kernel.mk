# i386 kernel makefile
# included from kernel.mk
KERNEL_OBJS += \
	$(KERNEL_ARCH_DIR)/arch_console.o \
	$(KERNEL_ARCH_DIR)/arch_cpu.o \
	$(KERNEL_ARCH_DIR)/arch_dbg_console.o \
	$(KERNEL_ARCH_DIR)/arch_debug.o \
	$(KERNEL_ARCH_DIR)/arch_faults.o \
	$(KERNEL_ARCH_DIR)/arch_i386.o \
	$(KERNEL_ARCH_DIR)/arch_interrupts.o \
	$(KERNEL_ARCH_DIR)/arch_int.o \
	$(KERNEL_ARCH_DIR)/arch_keyboard.o \
	$(KERNEL_ARCH_DIR)/arch_pmap.o \
	$(KERNEL_ARCH_DIR)/arch_smp.o \
	$(KERNEL_ARCH_DIR)/arch_timer.o \
	$(KERNEL_ARCH_DIR)/arch_thread.o \
	$(KERNEL_ARCH_DIR)/arch_vm.o
