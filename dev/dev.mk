DEV_DIR = dev
DEV_OBJ_DIR = $(DEV_DIR)/$(OBJ_DIR)
DEV_OBJS = \
	$(DEV_OBJ_DIR)/devs.o

DEV_INCLUDES = -Iinclude -Iboot/$(ARCH) -I$(DEV_DIR)
DEV_SUB_INCLUDES =

include $(DEV_DIR)/common/common.mk

DEV_ARCH_DIR = $(DEV_DIR)/arch/$(ARCH)
include $(DEV_ARCH_DIR)/arch_dev.mk

DEPS += $(DEV_OBJS:.o=.d)

DEV = $(DEV_OBJ_DIR)/dev.a

$(DEV): $(DEV_OBJS)
	$(AR) r $@ $(DEV_OBJS)

devclean:
	rm -f $(DEV_OBJS) $(DEV)

CLEAN += devclean

# build prototypes
$(DEV_OBJ_DIR)/%.o: $(DEV_DIR)/%.c
	@mkdir -p $(DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) $(DEV_SUB_INCLUDES) -o $@

$(DEV_OBJ_DIR)/%.d: $(DEV_DIR)/%.c
	@mkdir -p $(DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@(echo -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) $(DEV_SUB_INCLUDES) -M -MG $<) > $@

$(DEV_OBJ_DIR)/%.d: $(DEV_DIR)/%.S
	@mkdir -p $(DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) $(DEV_SUB_INCLUDES) -M -MG $<) > $@

$(DEV_OBJ_DIR)/%.o: $(DEV_DIR)/%.S
	@mkdir -p $(DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) $(DEV_SUB_INCLUDES) -o $@
