KERNEL_DIR = kernel
KERNEL_OBJ_DIR = kernel/$(OBJ_DIR)
KERNEL_LIB = $(KERNEL_OBJ_DIR)/kernel.o
KERNEL_OBJS = \
        $(KERNEL_OBJ_DIR)/main.o \
        $(KERNEL_OBJ_DIR)/elf.o \
        $(KERNEL_OBJ_DIR)/faults.o \
        $(KERNEL_OBJ_DIR)/khash.o \
        $(KERNEL_OBJ_DIR)/int.o \
        $(KERNEL_OBJ_DIR)/console.o \
        $(KERNEL_OBJ_DIR)/debug.o \
        $(KERNEL_OBJ_DIR)/dev.o \
        $(KERNEL_OBJ_DIR)/timer.o \
        $(KERNEL_OBJ_DIR)/sem.o \
        $(KERNEL_OBJ_DIR)/smp.o \
        $(KERNEL_OBJ_DIR)/thread.o \
        $(KERNEL_OBJ_DIR)/vfs.o \
        $(KERNEL_OBJ_DIR)/vm.o

KERNEL_INCLUDES = -Iinclude -Idev -Iboot/$(ARCH) -I$(KERNEL_DIR) -I$(KERNEL_ARCH_DIR)

include $(KERNEL_DIR)/fs/fs_kernel.mk

KERNEL_ARCH_DIR = kernel/arch/$(ARCH)
include $(KERNEL_ARCH_DIR)/arch_kernel.mk

DEPS += $(KERNEL_OBJS:.o=.d)

KERNEL = $(KERNEL_OBJ_DIR)/system

$(KERNEL): $(KERNEL_LIB) $(LIBS) $(DEV)
	$(LD) -dN --script=$(KERNEL_ARCH_DIR)/kernel.ld -L $(LIBGCC_PATH) -o $@ $(KERNEL_LIB) $(KLIBS) $(DEV) $(LIBGCC)

$(KERNEL_LIB): $(KERNEL_OBJS)
	$(LD) $(GLOBAL_LDFLAGS) -r -o $@ $(KERNEL_OBJS)

kernelclean:
	rm -f $(KERNEL_OBJS) $(KERNEL_LIB)

CLEAN += kernelclean

# build prototypes - this covers architecture dependant subdirs

$(KERNEL_OBJ_DIR)/%.o: $(KERNEL_DIR)/%.c
	@mkdir -p $(KERNEL_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) -o $@

$(KERNEL_OBJ_DIR)/%.d: $(KERNEL_DIR)/%.c
	@mkdir -p $(KERNEL_OBJ_DIR)
	@echo "making deps for $<..."
	@(echo -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) -M -MG $<) > $@

$(KERNEL_OBJ_DIR)/%.d: $(KERNEL_DIR)/%.S
	@mkdir -p $(KERNEL_OBJ_DIR)
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) -M -MG $<) > $@

$(KERNEL_OBJ_DIR)/%.o: $(KERNEL_DIR)/%.S
	@mkdir -p $(KERNEL_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) -o $@
