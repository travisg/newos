KERNEL_OBJS += \
		$(KERNEL_DIR)/fs/rootfs.o

KERNEL_FS_DIR = kernel/fs

KERNEL_FS_INCLUDES = -I$(KERNEL_FS_DIR)

$(KERNEL_FS_DIR)/%.o: $(KERNEL_FS_DIR)/%.c
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) $(KERNEL_FS_INCLUDES) -o $@

$(KERNEL_FS_DIR)/%.d: $(KERNEL_FS_DIR)/%.c
	@echo "making deps for $<..."
	@(echo -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) $(KERNEL_FS_INCLUDES) -M -MG $<) > $@

$(KERNEL_FS_DIR)/%.d: $(KERNEL_FS_DIR)/%.S
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) $(KERNEL_FS_INCLUDES) -M -MG $<) > $@

$(KERNEL_FS_DIR)/%.o: $(KERNEL_FS_DIR)/%.S
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_INCLUDES) $(KERNEL_FS_INCLUDES) -o $@
