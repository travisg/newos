CONSOLE_DEV_DIR = $(DEV_ARCH_DIR)/console
CONSOLE_DEV_OBJ_DIR = $(CONSOLE_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(CONSOLE_DEV_OBJ_DIR)/console.o \
	$(CONSOLE_DEV_OBJ_DIR)/keyboard.o

DEV_SUB_INCLUDES += \
	-I$(DEV_ARCH_DIR)/console

$(CONSOLE_DEV_OBJ_DIR)/%.o: $(CONSOLE_DEV_DIR)/%.c
	@mkdir -p $(CONSOLE_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@

$(CONSOLE_DEV_OBJ_DIR)/%.d: $(CONSOLE_DEV_DIR)/%.c
	@mkdir -p $(CONSOLE_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(CONSOLE_DEV_OBJ_DIR)/%.d: $(CONSOLE_DEV_DIR)/%.S
	@mkdir -p $(CONSOLE_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(CONSOLE_DEV_OBJ_DIR)/%.o: $(CONSOLE_DEV_DIR)/%.S
	@mkdir -p $(CONSOLE_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@
