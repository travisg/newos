FBCONSOLE_DEV_DIR = $(DEV_DIR)/fb_console
FBCONSOLE_DEV_OBJ_DIR = $(FBCONSOLE_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(FBCONSOLE_DEV_OBJ_DIR)/fb_console.o 

DEV_SUB_INCLUDES += \
	-I$(DEV_DIR)/fb_console

$(FBCONSOLE_DEV_OBJ_DIR)/%.o: $(FBCONSOLE_DEV_DIR)/%.c
	@mkdir -p $(FBCONSOLE_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -o $@

$(FBCONSOLE_DEV_OBJ_DIR)/%.d: $(FBCONSOLE_DEV_DIR)/%.c
	@mkdir -p $(FBCONSOLE_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(FBCONSOLE_DEV_OBJ_DIR)/%.d: $(FBCONSOLE_DEV_DIR)/%.S
	@mkdir -p $(FBCONSOLE_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(FBCONSOLE_DEV_OBJ_DIR)/%.o: $(FBCONSOLE_DEV_DIR)/%.S
	@mkdir -p $(FBCONSOLE_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -o $@
