ifneq ($(_KERNEL_MAKE),1)
_KERNEL_MAKE = 1

KERNEL_DIR = kernel

# include targets we depend on
include lib/lib.mk
include $(KERNEL_DIR)/bus/bus.mk
include $(KERNEL_DIR)/dev/dev.mk

KERNEL_OBJ_DIR = kernel/$(OBJ_DIR)
KERNEL_OBJS = \
        $(KERNEL_OBJ_DIR)/main.o \
        $(KERNEL_OBJ_DIR)/elf.o \
        $(KERNEL_OBJ_DIR)/faults.o \
        $(KERNEL_OBJ_DIR)/khash.o \
        $(KERNEL_OBJ_DIR)/lock.o \
        $(KERNEL_OBJ_DIR)/heap.o \
        $(KERNEL_OBJ_DIR)/int.o \
        $(KERNEL_OBJ_DIR)/console.o \
        $(KERNEL_OBJ_DIR)/debug.o \
        $(KERNEL_OBJ_DIR)/gdb.o \
        $(KERNEL_OBJ_DIR)/dev.o \
        $(KERNEL_OBJ_DIR)/queue.o \
        $(KERNEL_OBJ_DIR)/timer.o \
        $(KERNEL_OBJ_DIR)/port.o \
        $(KERNEL_OBJ_DIR)/sem.o \
        $(KERNEL_OBJ_DIR)/smp.o \
        $(KERNEL_OBJ_DIR)/syscalls.o \
        $(KERNEL_OBJ_DIR)/thread.o \
        $(KERNEL_OBJ_DIR)/cbuf.o \
        $(KERNEL_OBJ_DIR)/cpu.o \
        $(KERNEL_OBJ_DIR)/vfs.o

KERNEL_INCLUDES = -Iinclude

include $(KERNEL_DIR)/fs/fs_kernel.mk
include $(KERNEL_DIR)/vm/vm_kernel.mk
include $(KERNEL_DIR)/net/net_kernel.mk

KERNEL_ARCH_DIR = kernel/arch/$(ARCH)
include $(KERNEL_ARCH_DIR)/arch_kernel.mk

DEPS += $(KERNEL_OBJS:.o=.d)

KERNEL = $(KERNEL_OBJ_DIR)/system
LIBKERNEL = $(KERNEL_OBJ_DIR)/libsystem.so
LINKHACK = $(KERNEL_OBJ_DIR)/linkhack.so

kernel:	$(KERNEL) $(LIBKERNEL)

$(KERNEL): $(KERNEL_OBJS) $(KLIBS) $(DEV) $(BUS) $(LINKHACK)
	$(LD) $(GLOBAL_LDFLAGS) -Bdynamic -export-dynamic -dynamic-linker /foo/bar -T $(KERNEL_ARCH_DIR)/kernel.ld -L $(LIBGCC_PATH) -o $@ $(KERNEL_OBJS) $(DEV) $(BUS) $(LINKHACK) $(LINK_KLIBS) $(LIBGCC)

$(LIBKERNEL): $(KERNEL_OBJS) $(KLIBS) $(DEV) $(BUS) $(LINKHACK)
	$(LD) $(GLOBAL_LDFLAGS) -Bdynamic -shared -export-dynamic -dynamic-linker /foo/bar -T $(KERNEL_ARCH_DIR)/kernel.ld -L $(LIBGCC_PATH) -o $@ $(KERNEL_OBJS) $(DEV) $(BUS) $(LINKHACK) $(LINK_KLIBS) $(LIBGCC)

# Build a shared object with nothing in it to link against the kernel.
# Otherwise the linker optimizes and removes the dynamic section.
# This allows us to build a kernel with a dynamic section but not relocatable.
# Ttolen from the FreeBSD sys makefile.
$(LINKHACK): $(KERNEL_DIR)/linkhack.c
	$(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) -shared -nostdlib $< -o $@

DEPS += $(KERNEL_OBJ_DIR)/linkhack.d

kernelclean:
	rm -f $(KERNEL_OBJS) $(LINKHACK) $(KERNEL) $(LIBKERNEL)

CLEAN += kernelclean

include $(KERNEL_DIR)/addons/addons.mk

# build prototypes - this covers architecture dependant subdirs

$(KERNEL_OBJ_DIR)/%.o: $(KERNEL_DIR)/%.c
	@if [ ! -d $(KERNEL_OBJ_DIR) ]; then mkdir -p $(KERNEL_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(KERNEL_INCLUDES) -o $@

$(KERNEL_OBJ_DIR)/%.d: $(KERNEL_DIR)/%.c
	@if [ ! -d $(KERNEL_OBJ_DIR) ]; then mkdir -p $(KERNEL_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(KERNEL_INCLUDES) -M -MG $<) > $@

endif

