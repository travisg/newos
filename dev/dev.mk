DEV_DIR = dev
DEV_OBJS = \
	$(DEV_DIR)/devs.o \
	$(DEV_DIR)/common/null.o \
	$(DEV_DIR)/common/zero.o

DEV_ARCH_DIR = $(DEV_DIR)/arch/$(ARCH)
include $(DEV_ARCH_DIR)/arch_dev.mk

DEPS += $(DEV_OBJS:.o=.d)

DEV = $(DEV_DIR)/dev.a

$(DEV): $(DEV_OBJS)
	$(AR) r $@ $(DEV_OBJS)

devclean:
	rm -f $(DEV_OBJS) $(DEV)

CLEAN += devclean

# build prototypes
DEV_INCLUDES = -Iinclude -Iboot/$(ARCH) -I$(KERNEL_DIR) -I$(KERNEL_ARCH_DIR) -I$(DEV_DIR) -I$(DEV_DIR)/common $(DEV_ARCH_INCLUDES)

$(DEV_DIR)/%.o: $(DEV_DIR)/%.c
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@

$(DEV_DIR)/%.d: $(DEV_DIR)/%.c
	@echo "making deps for $<..."
	@(echo -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(DEV_DIR)/%.d: $(DEV_DIR)/%.S
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(DEV_DIR)/%.o: $(DEV_DIR)/%.S
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@
