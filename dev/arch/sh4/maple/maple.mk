PCI_DEV_DIR = $(DEV_ARCH_DIR)/maple
PCI_DEV_OBJ_DIR = $(PCI_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(PCI_DEV_OBJ_DIR)/maple.o

DEV_SUB_INCLUDES += \
	-I$(DEV_ARCH_DIR)/maple

$(PCI_DEV_OBJ_DIR)/%.o: $(PCI_DEV_DIR)/%.c
	@mkdir -p $(PCI_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@

$(PCI_DEV_OBJ_DIR)/%.d: $(PCI_DEV_DIR)/%.c
	@mkdir -p $(PCI_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(PCI_DEV_OBJ_DIR)/%.d: $(PCI_DEV_DIR)/%.S
	@mkdir -p $(PCI_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(PCI_DEV_OBJ_DIR)/%.o: $(PCI_DEV_DIR)/%.S
	@mkdir -p $(PCI_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@
