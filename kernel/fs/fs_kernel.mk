KERNEL_FS_DIR = $(KERNEL_DIR)/fs
KERNEL_FS_OBJ_DIR = $(KERNEL_FS_DIR)/$(OBJ_DIR)
KERNEL_OBJS += \
		$(KERNEL_FS_OBJ_DIR)/rootfs.o \
		$(KERNEL_FS_OBJ_DIR)/bootfs.o

KERNEL_FS_INCLUDES = $(KERNEL_INCLUDES)

$(KERNEL_FS_OBJ_DIR)/%.o: $(KERNEL_FS_DIR)/%.c
	@mkdir -p $(KERNEL_FS_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_FS_INCLUDES) -o $@

$(KERNEL_FS_OBJ_DIR)/%.d: $(KERNEL_FS_DIR)/%.c
	@mkdir -p $(KERNEL_FS_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_FS_INCLUDES) -M -MG $<) > $@
