ifneq ($(_DEV_MAKE),1)
_DEV_MAKE = 1

DEV_DIR = dev
DEV_OBJ_DIR = $(DEV_DIR)/$(OBJ_DIR)
DEV_OBJS = \
	$(DEV_OBJ_DIR)/devs.o

DEV_INCLUDES = -Iinclude
DEV_SUB_INCLUDES =

include $(DEV_DIR)/common/common.mk

DEV_ARCH_DIR = $(DEV_DIR)/arch/$(ARCH)
include $(DEV_ARCH_DIR)/arch_dev.mk

DEPS += $(DEV_OBJS:.o=.d)

DEV = $(DEV_OBJ_DIR)/dev.o

$(DEV): $(DEV_OBJS)
	$(LD) -r -o $@ $(DEV_OBJS)

devs: $(DEV)

devclean:
	rm -f $(DEV_OBJS) $(DEV)

CLEAN += devclean

# build prototypes
$(DEV_OBJ_DIR)/%.o: $(DEV_DIR)/%.c
	@if [ ! -d $(DEV_OBJ_DIR) ]; then mkdir -p $(DEV_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) $(DEV_SUB_INCLUDES) -o $@

$(DEV_OBJ_DIR)/%.d: $(DEV_DIR)/%.c
	@if [ ! -d $(DEV_OBJ_DIR) ]; then mkdir -p $(DEV_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) $(DEV_SUB_INCLUDES) -M -MG $<) > $@

$(DEV_OBJ_DIR)/%.d: $(DEV_DIR)/%.S
	@if [ ! -d $(DEV_OBJ_DIR) ]; then mkdir -p $(DEV_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) $(DEV_SUB_INCLUDES) -M -MG $<) > $@

$(DEV_OBJ_DIR)/%.o: $(DEV_DIR)/%.S
	@if [ ! -d $(DEV_OBJ_DIR) ]; then mkdir -p $(DEV_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) $(DEV_SUB_INCLUDES) -o $@
endif
