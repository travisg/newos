NULIBC_SYSTEM_DIR = $(NULIBC_DIR)/system
NULIBC_SYSTEM_OBJ_DIR = $(NULIBC_SYSTEM_DIR)/$(OBJ_DIR)

NULIBC_SYSTEM_OBJS = \
	$(NULIBC_SYSTEM_OBJ_DIR)/dlfcn.o \
	$(NULIBC_SYSTEM_OBJ_DIR)/syscalls.o

DEPS += $(NULIBC_SYSTEM_OBJS:.o=.d)

include $(NULIBC_SYSTEM_DIR)/arch/$(ARCH)/nulibc_system_arch.mk

# build prototypes
$(NULIBC_SYSTEM_OBJ_DIR)/%.o: $(NULIBC_SYSTEM_DIR)/%.c 
	@if [ ! -d $(NULIBC_SYSTEM_OBJ_DIR) ]; then mkdir -p $(NULIBC_SYSTEM_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_SYSTEM_OBJ_DIR)/%.d: $(NULIBC_SYSTEM_DIR)/%.c
	@if [ ! -d $(NULIBC_SYSTEM_OBJ_DIR) ]; then mkdir -p $(NULIBC_SYSTEM_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_SYSTEM_OBJ_DIR)/%.d: $(NULIBC_SYSTEM_DIR)/%.S
	@if [ ! -d $(NULIBC_SYSTEM_OBJ_DIR) ]; then mkdir -p $(NULIBC_SYSTEM_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_SYSTEM_OBJ_DIR)/%.o: $(NULIBC_SYSTEM_DIR)/%.S
	@if [ ! -d $(NULIBC_SYSTEM_OBJ_DIR) ]; then mkdir -p $(NULIBC_SYSTEM_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

