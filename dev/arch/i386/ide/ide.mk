IDE_DEV_DIR = $(DEV_ARCH_DIR)/ide
IDE_DEV_OBJ_DIR = $(IDE_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(IDE_DEV_OBJ_DIR)/ide.o \
	$(IDE_DEV_OBJ_DIR)/ide_raw.o

DEV_SUB_INCLUDES += \
	-I$(DEV_ARCH_DIR)/ide

$(IDE_DEV_OBJ_DIR)/%.o: $(IDE_DEV_DIR)/%.c
	@mkdir -p $(IDE_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@

$(IDE_DEV_OBJ_DIR)/%.d: $(IDE_DEV_DIR)/%.c
	@mkdir -p $(IDE_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(IDE_DEV_OBJ_DIR)/%.d: $(IDE_DEV_DIR)/%.S
	@mkdir -p $(IDE_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(IDE_DEV_OBJ_DIR)/%.o: $(IDE_DEV_DIR)/%.S
	@mkdir -p $(IDE_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@
