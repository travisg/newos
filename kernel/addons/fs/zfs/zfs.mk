ZFS_DIR = $(KERNEL_ADDONS_DIR)/fs/zfs
ZFS_OBJ_DIR = $(ZFS_DIR)/$(OBJ_DIR)
ZFS_OBJS = \
        $(ZFS_OBJ_DIR)/zfs.o

DEPS += $(ZFS_OBJS:.o=.d)

ZFS_ADDON = $(ZFS_OBJ_DIR)/zfs

KERNEL_ADDONS += $(ZFS_ADDON)

$(ZFS_ADDON): $(ZFS_OBJS) $(LIBKERNEL)
	$(LD) -Bdynamic -shared -T $(KERNEL_ARCH_DIR)/addon.ld -L $(LIBGCC_PATH) -L $(KERNEL_OBJ_DIR) -o $@ $(ZFS_OBJS)  $(LIBGCC) -lsystem

zfsclean:
	rm -f $(ZFS_OBJS) $(ZFS_ADDON)

CLEAN += zfsclean

# build prototypes - this covers architecture dependant subdirs

$(ZFS_OBJ_DIR)/%.o: $(ZFS_DIR)/%.c
	@if [ ! -d $(ZFS_OBJ_DIR) ]; then mkdir -p $(ZFS_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(KERNEL_ADDONS_CFLAGS) $(KERNEL_ADDONS_INCLUDES) -o $@

$(ZFS_OBJ_DIR)/%.d: $(ZFS_DIR)/%.c
	@if [ ! -d $(ZFS_OBJ_DIR) ]; then mkdir -p $(ZFS_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(KERNEL_ADDONS_CFLAGS) $(KERNEL_ADDONS_INCLUDES) -M -MG $<) > $@
