IDE3_DEV_DIR = $(DEV_ARCH_DIR)/ide3
IDE3_DEV_OBJ_DIR = $(IDE3_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(IDE3_DEV_OBJ_DIR)/ide.o \
	$(IDE3_DEV_OBJ_DIR)/ide_raw.o

DEV_SUB_INCLUDES += \
	-I$(DEV_ARCH_DIR)/ide3

$(IDE3_DEV_OBJ_DIR)/%.o: $(IDE3_DEV_DIR)/%.c
	@mkdir -p $(IDE3_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -o $@

$(IDE3_DEV_OBJ_DIR)/%.d: $(IDE3_DEV_DIR)/%.c
	@mkdir -p $(IDE3_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(IDE3_DEV_OBJ_DIR)/%.d: $(IDE3_DEV_DIR)/%.S
	@mkdir -p $(IDE3_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(IDE3_DEV_OBJ_DIR)/%.o: $(IDE3_DEV_DIR)/%.S
	@mkdir -p $(IDE3_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -o $@
