LIBSYS_ARCH_OBJ_DIR = $(LIBSYS_ARCH_DIR)/$(OBJ_DIR)
LIBSYS_OBJS += \
	$(LIBSYS_ARCH_OBJ_DIR)/syscalls.o 

# build prototypes
$(LIBSYS_ARCH_OBJ_DIR)/%.o: $(LIBSYS_ARCH_DIR)/%.c 
	@mkdir -p $(LIBSYS_ARCH_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

$(LIBSYS_ARCH_OBJ_DIR)/%.d: $(LIBSYS_ARCH_DIR)/%.c
	@mkdir -p $(LIBSYS_ARCH_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBSYS_ARCH_OBJ_DIR)/%.d: $(LIBSYS_ARCH_DIR)/%.S
	@mkdir -p $(LIBSYS_ARCH_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBSYS_ARCH_OBJ_DIR)/%.o: $(LIBSYS_ARCH_DIR)/%.S
	@mkdir -p $(LIBSYS_ARCH_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

