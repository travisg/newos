RTL8139_DEV_DIR = $(DEV_ARCH_DIR)/rtl8139
RTL8139_DEV_OBJ_DIR = $(RTL8139_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(RTL8139_DEV_OBJ_DIR)/rtl8139.o \
	$(RTL8139_DEV_OBJ_DIR)/rtl8139_dev.o

DEV_SUB_INCLUDES += \
	-I$(DEV_ARCH_DIR)/rtl8139

$(RTL8139_DEV_OBJ_DIR)/%.o: $(RTL8139_DEV_DIR)/%.c
	@mkdir -p $(RTL8139_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@

$(RTL8139_DEV_OBJ_DIR)/%.d: $(RTL8139_DEV_DIR)/%.c
	@mkdir -p $(RTL8139_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(RTL8139_DEV_OBJ_DIR)/%.d: $(RTL8139_DEV_DIR)/%.S
	@mkdir -p $(RTL8139_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(RTL8139_DEV_OBJ_DIR)/%.o: $(RTL8139_DEV_DIR)/%.S
	@mkdir -p $(RTL8139_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@
