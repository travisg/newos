ISO9660FS_DIR = $(KERNEL_ADDONS_DIR)/fs/iso9660
ISO9660FS_OBJ_DIR = $(ISO9660FS_DIR)/$(OBJ_DIR)
ISO9660FS_OBJS = \
        $(ISO9660FS_OBJ_DIR)/isofs.o

DEPS += $(ISO9660FS_OBJS:.o=.d)

ISO9660FS_ADDON = $(ISO9660FS_OBJ_DIR)/iso9660fs

KERNEL_ADDONS += $(ISO9660FS_ADDON)

$(ISO9660FS_ADDON): $(ISO9660FS_OBJS) $(LIBKERNEL)
	$(LD) -Bdynamic -shared -T $(KERNEL_ARCH_DIR)/addon.ld -L $(LIBGCC_PATH) -L $(KERNEL_OBJ_DIR) -o $@ $(ISO9660FS_OBJS)  $(LIBGCC) -lsystem

iso9660clean:
	rm -f $(ISO9660FS_OBJS) $(ISO9660FS_ADDON)

CLEAN += iso9660clean

# build prototypes - this covers architecture dependant subdirs

$(ISO9660FS_OBJ_DIR)/%.o: $(ISO9660FS_DIR)/%.c
	@if [ ! -d $(ISO9660FS_OBJ_DIR) ]; then mkdir -p $(ISO9660FS_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_ADDONS_CFLAGS) $(KERNEL_ADDONS_INCLUDES) -o $@

$(ISO9660FS_OBJ_DIR)/%.d: $(ISO9660FS_DIR)/%.c
	@if [ ! -d $(ISO9660FS_OBJ_DIR) ]; then mkdir -p $(ISO9660FS_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_ADDONS_CFLAGS) $(KERNEL_ADDONS_INCLUDES) -M -MG $<) > $@
