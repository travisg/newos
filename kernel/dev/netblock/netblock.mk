NETBLOCK_DEV_DIR = $(DEV_DIR)/netblock
NETBLOCK_DEV_OBJ_DIR = $(NETBLOCK_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(NETBLOCK_DEV_OBJ_DIR)/netblock.o 

DEV_SUB_INCLUDES += \
	-I$(DEV_DIR)/netblock

$(NETBLOCK_DEV_OBJ_DIR)/%.o: $(NETBLOCK_DEV_DIR)/%.c
	@mkdir -p $(NETBLOCK_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -o $@

$(NETBLOCK_DEV_OBJ_DIR)/%.d: $(NETBLOCK_DEV_DIR)/%.c
	@mkdir -p $(NETBLOCK_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(NETBLOCK_DEV_OBJ_DIR)/%.d: $(NETBLOCK_DEV_DIR)/%.S
	@mkdir -p $(NETBLOCK_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(NETBLOCK_DEV_OBJ_DIR)/%.o: $(NETBLOCK_DEV_DIR)/%.S
	@mkdir -p $(NETBLOCK_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_CFLAGS) $(DEV_INCLUDES) -o $@
