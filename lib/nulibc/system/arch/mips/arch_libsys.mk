LIBSYS_ARCH_OBJ_DIR = $(LIBSYS_ARCH_DIR)/$(OBJ_DIR)
LIBSYS_OBJS += 

# build prototypes
$(LIBSYS_ARCH_OBJ_DIR)/%.o: $(LIBSYS_ARCH_DIR)/%.c 
	@if [ ! -d $(LIBSYS_ARCH_OBJ_DIR) ]; then mkdir -p $(LIBSYS_ARCH_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

$(LIBSYS_ARCH_OBJ_DIR)/%.d: $(LIBSYS_ARCH_DIR)/%.c
	@if [ ! -d $(LIBSYS_ARCH_OBJ_DIR) ]; then mkdir -p $(LIBSYS_ARCH_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBSYS_ARCH_OBJ_DIR)/%.d: $(LIBSYS_ARCH_DIR)/%.S
	@if [ ! -d $(LIBSYS_ARCH_OBJ_DIR) ]; then mkdir -p $(LIBSYS_ARCH_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBSYS_ARCH_OBJ_DIR)/%.o: $(LIBSYS_ARCH_DIR)/%.S
	@if [ ! -d $(LIBSYS_ARCH_OBJ_DIR) ]; then mkdir -p $(LIBSYS_ARCH_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@
