MOUSE_DEV_DIR = $(DEV_ARCH_DIR)/ps2mouse
MOUSE_DEV_OBJ_DIR = $(MOUSE_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(MOUSE_DEV_OBJ_DIR)/ps2mouse.o

DEV_SUB_INCLUDES += \
	-I$(DEV_ARCH_DIR)/ps2mouse

$(MOUSE_DEV_OBJ_DIR)/%.o: $(MOUSE_DEV_DIR)/%.c
	@mkdir -p $(MOUSE_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -o $@

$(MOUSE_DEV_OBJ_DIR)/%.d: $(MOUSE_DEV_DIR)/%.c
	@mkdir -p $(MOUSE_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(MOUSE_DEV_OBJ_DIR)/%.d: $(MOUSE_DEV_DIR)/%.S
	@mkdir -p $(MOUSE_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(MOUSE_DEV_OBJ_DIR)/%.o: $(MOUSE_DEV_DIR)/%.S
	@mkdir -p $(MOUSE_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -o $@
