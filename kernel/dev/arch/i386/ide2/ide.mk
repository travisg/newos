IDE2_DEV_DIR = $(DEV_ARCH_DIR)/ide2
IDE2_DEV_OBJ_DIR = $(IDE2_DEV_DIR)/$(OBJ_DIR)
DEV_OBJS += \
	$(IDE2_DEV_OBJ_DIR)/ide_drive.o \
	$(IDE2_DEV_OBJ_DIR)/ide_bus.o \
	$(IDE2_DEV_OBJ_DIR)/isa_ide_bus.o \
	$(IDE2_DEV_OBJ_DIR)/pci_ide_bus.o \
	$(IDE2_DEV_OBJ_DIR)/ide.o \
	$(IDE2_DEV_OBJ_DIR)/ata_ide_drive.o \
	$(IDE2_DEV_OBJ_DIR)/atapi_ide_drive.o \
	

DEV_SUB_INCLUDES += \
	-I$(DEV_ARCH_DIR)/ide2

$(IDE2_DEV_OBJ_DIR)/%.o: $(IDE2_DEV_DIR)/%.c
	@mkdir -p $(IDE2_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@

$(IDE2_DEV_OBJ_DIR)/%.d: $(IDE2_DEV_DIR)/%.c
	@mkdir -p $(IDE2_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(IDE2_DEV_OBJ_DIR)/%.d: $(IDE2_DEV_DIR)/%.S
	@mkdir -p $(IDE2_DEV_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -M -MG $<) > $@

$(IDE2_DEV_OBJ_DIR)/%.o: $(IDE2_DEV_DIR)/%.S
	@mkdir -p $(IDE2_DEV_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(DEV_INCLUDES) -o $@
