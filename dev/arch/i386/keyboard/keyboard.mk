KEYBOARD_DEV_DIR = $(DEV_ARCH_DIR)/keyboard
KEYBOARD_DEV_OBJ_DIR = $(KEYBOARD_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(KEYBOARD_DEV_OBJ_DIR)/keyboard.o

DEV_SUB_INCLUDES += \
	-I$(DEV_ARCH_DIR)/keyboard

$(KEYBOARD_DEV_OBJ_DIR)/%.o: $(KEYBOARD_DEV_DIR)/%.c
	@mkdir -p $(KEYBOARD_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@

$(KEYBOARD_DEV_OBJ_DIR)/%.d: $(KEYBOARD_DEV_DIR)/%.c
	@mkdir -p $(KEYBOARD_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(KEYBOARD_DEV_OBJ_DIR)/%.d: $(KEYBOARD_DEV_DIR)/%.S
	@mkdir -p $(KEYBOARD_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(KEYBOARD_DEV_OBJ_DIR)/%.o: $(KEYBOARD_DEV_DIR)/%.S
	@mkdir -p $(KEYBOARD_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@
