NULIBC_SYSTEM_ARCH_DIR = $(NULIBC_DIR)/system/arch/$(ARCH)
NULIBC_SYSTEM_ARCH_OBJ_DIR = $(NULIBC_SYSTEM_ARCH_DIR)/$(OBJ_DIR)

NULIBC_SYSTEM_ARCH_OBJS = \

DEPS += $(NULIBC_SYSTEM_ARCH_OBJS:.o=.d)

NULIBC_SYSTEM_OBJS += $(NULIBC_SYSTEM_ARCH_OBJS)

# build prototypes
$(NULIBC_SYSTEM_ARCH_OBJ_DIR)/%.o: $(NULIBC_SYSTEM_ARCH_DIR)/%.c 
	@if [ ! -d $(NULIBC_SYSTEM_ARCH_OBJ_DIR) ]; then mkdir -p $(NULIBC_SYSTEM_ARCH_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_SYSTEM_ARCH_OBJ_DIR)/%.d: $(NULIBC_SYSTEM_ARCH_DIR)/%.c
	@if [ ! -d $(NULIBC_SYSTEM_ARCH_OBJ_DIR) ]; then mkdir -p $(NULIBC_SYSTEM_ARCH_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_SYSTEM_ARCH_OBJ_DIR)/%.d: $(NULIBC_SYSTEM_ARCH_DIR)/%.S
	@if [ ! -d $(NULIBC_SYSTEM_ARCH_OBJ_DIR) ]; then mkdir -p $(NULIBC_SYSTEM_ARCH_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_SYSTEM_ARCH_OBJ_DIR)/%.o: $(NULIBC_SYSTEM_ARCH_DIR)/%.S
	@if [ ! -d $(NULIBC_SYSTEM_ARCH_OBJ_DIR) ]; then mkdir -p $(NULIBC_SYSTEM_ARCH_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

