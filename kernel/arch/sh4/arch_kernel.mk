# sh4 kernel makefile
# included from kernel.mk
KERNEL_OBJS += \
	$(KERNEL_ARCH_DIR)/arch_faults.o \
	$(KERNEL_ARCH_DIR)/arch_console.o \
	$(KERNEL_ARCH_DIR)/arch_dbg_console.o \
	$(KERNEL_ARCH_DIR)/arch_pmap.o \
	$(KERNEL_ARCH_DIR)/arch_vm.o
