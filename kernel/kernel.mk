KERNEL_DIR = kernel
KERNEL_LIB = $(KERNEL_DIR)/kernel.o
KERNEL_OBJS = \
        $(KERNEL_DIR)/main.o \
        $(KERNEL_DIR)/faults.o \
        $(KERNEL_DIR)/khash.o \
        $(KERNEL_DIR)/int.o \
        $(KERNEL_DIR)/console.o \
        $(KERNEL_DIR)/debug.o \
        $(KERNEL_DIR)/dev.o \
        $(KERNEL_DIR)/timer.o \
        $(KERNEL_DIR)/proc.o \
        $(KERNEL_DIR)/sem.o \
        $(KERNEL_DIR)/smp.o \
        $(KERNEL_DIR)/thread.o \
        $(KERNEL_DIR)/vfs.o \
        $(KERNEL_DIR)/vm.o

include $(KERNEL_DIR)/fs/fs_kernel.mk

KERNEL_ARCH_DIR = kernel/arch/$(ARCH)
include $(KERNEL_ARCH_DIR)/arch_kernel.mk

KERNEL_DEPS = $(KERNEL_OBJS:.o=.d)

KERNEL = $(KERNEL_DIR)/system

$(KERNEL): $(KERNEL_LIB) $(LIBS) $(DEV)
	$(LD) -dN --script=$(KERNEL_ARCH_DIR)/kernel.ld -L $(LIBGCC_PATH) -o $@ $(KERNEL_LIB) $(LIBS) $(DEV) $(LIBGCC)

$(KERNEL_LIB): $(KERNEL_OBJS)
	$(LD) $(GLOBAL_LDFLAGS) -r -o $@ $(KERNEL_OBJS)

kernelclean:
	rm -f $(KERNEL_OBJS) $(KERNEL_LIB)

include $(KERNEL_DEPS)

# build prototypes - this covers architecture dependant subdirs
KERNEL_INCLUDES = -Iinclude -Idev -Iboot/$(ARCH) -I$(KERNEL_DIR) -I$(KERNEL_ARCH_DIR)

$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) -o $@

$(KERNEL_DIR)/%.d: $(KERNEL_DIR)/%.c
	@echo "making deps for $<..."
	@(echo -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) -M -MG $<) > $@

$(KERNEL_DIR)/%.d: $(KERNEL_DIR)/%.S
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) -M -MG $<) > $@

$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.S
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) -o $@
