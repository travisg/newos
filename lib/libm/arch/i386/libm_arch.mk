LIBM_ARCH_DIR = $(LIBM_DIR)/arch/$(ARCH)
LIBM_ARCH_OBJ_DIR = $(LIBM_ARCH_DIR)/$(OBJ_DIR)

LIBM_ARCH_OBJS = \
	$(LIBM_ARCH_OBJ_DIR)/fabs.o \
	$(LIBM_ARCH_OBJ_DIR)/frexp.o \
	$(LIBM_ARCH_OBJ_DIR)/isinf.o \
	$(LIBM_ARCH_OBJ_DIR)/ldexp.o

DEPS += $(LIBM_ARCH_OBJS:.o=.d)


# build prototypes
$(LIBM_ARCH_OBJ_DIR)/%.o: $(LIBM_ARCH_DIR)/%.c 
	@if [ ! -d $(LIBM_ARCH_OBJ_DIR) ]; then mkdir -p $(LIBM_ARCH_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -o $@

$(LIBM_ARCH_OBJ_DIR)/%.d: $(LIBM_ARCH_DIR)/%.c
	@if [ ! -d $(LIBM_ARCH_OBJ_DIR) ]; then mkdir -p $(LIBM_ARCH_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -M -MG $<) > $@

$(LIBM_ARCH_OBJ_DIR)/%.d: $(LIBM_ARCH_DIR)/%.S
	@if [ ! -d $(LIBM_ARCH_OBJ_DIR) ]; then mkdir -p $(LIBM_ARCH_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -M -MG $<) > $@

$(LIBM_ARCH_OBJ_DIR)/%.o: $(LIBM_ARCH_DIR)/%.S
	@if [ ! -d $(LIBM_ARCH_OBJ_DIR) ]; then mkdir -p $(LIBM_ARCH_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -o $@

