PCI_BUS_DIR = $(BUS_DIR)/pci
PCI_BUS_OBJ_DIR = $(PCI_BUS_DIR)/$(OBJ_DIR)
BUS_OBJS += \
	$(PCI_BUS_OBJ_DIR)/pci.o \
	$(PCI_BUS_OBJ_DIR)/pci_bus.o

BUS_SUB_INCLUDES += 

$(PCI_BUS_OBJ_DIR)/%.o: $(PCI_BUS_DIR)/%.c
	@mkdir -p $(PCI_BUS_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(BUS_INCLUDES) -o $@

$(PCI_BUS_OBJ_DIR)/%.d: $(PCI_BUS_DIR)/%.c
	@mkdir -p $(PCI_BUS_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(BUS_INCLUDES) -M -MG $<) > $@

$(PCI_BUS_OBJ_DIR)/%.d: $(PCI_BUS_DIR)/%.S
	@mkdir -p $(PCI_BUS_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(BUS_INCLUDES) -M -MG $<) > $@

$(PCI_BUS_OBJ_DIR)/%.o: $(PCI_BUS_DIR)/%.S
	@mkdir -p $(PCI_BUS_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(BUS_INCLUDES) -o $@
