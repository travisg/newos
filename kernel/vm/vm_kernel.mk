KERNEL_VM_DIR = $(KERNEL_DIR)/vm
KERNEL_VM_OBJ_DIR = $(KERNEL_VM_DIR)/$(OBJ_DIR)
KERNEL_OBJS += \
		$(KERNEL_VM_OBJ_DIR)/vm.o \
		$(KERNEL_VM_OBJ_DIR)/vm_cache.o \
		$(KERNEL_VM_OBJ_DIR)/vm_page.o \
		$(KERNEL_VM_OBJ_DIR)/vm_store_anonymous_noswap.o \
		$(KERNEL_VM_OBJ_DIR)/vm_store_device.o \
		$(KERNEL_VM_OBJ_DIR)/vm_tests.o		

KERNEL_VM_INCLUDES = $(KERNEL_INCLUDES)

$(KERNEL_VM_OBJ_DIR)/%.o: $(KERNEL_VM_DIR)/%.c
	@if [ ! -d $(KERNEL_VM_OBJ_DIR) ]; then mkdir -p $(KERNEL_VM_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_VM_INCLUDES) -o $@

$(KERNEL_VM_OBJ_DIR)/%.d: $(KERNEL_VM_DIR)/%.c
	@if [ ! -d $(KERNEL_VM_OBJ_DIR) ]; then mkdir -p $(KERNEL_VM_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_VM_INCLUDES) -M -MG $<) > $@
