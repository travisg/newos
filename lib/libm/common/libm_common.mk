LIBM_COMMON_DIR = $(LIBM_DIR)/common
LIBM_COMMON_OBJ_DIR = $(LIBM_COMMON_DIR)/$(OBJ_DIR)

LIBM_COMMON_OBJS = \
	$(LIBM_COMMON_OBJ_DIR)/atan2.o \
	$(LIBM_COMMON_OBJ_DIR)/sincos.o \
	$(LIBM_COMMON_OBJ_DIR)/tan.o

DEPS += $(LIBM_COMMON_OBJS:.o=.d)


# build prototypes
$(LIBM_COMMON_OBJ_DIR)/%.o: $(LIBM_COMMON_DIR)/%.c 
	@if [ ! -d $(LIBM_COMMON_OBJ_DIR) ]; then mkdir -p $(LIBM_COMMON_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -o $@

$(LIBM_COMMON_OBJ_DIR)/%.d: $(LIBM_COMMON_DIR)/%.c
	@if [ ! -d $(LIBM_COMMON_OBJ_DIR) ]; then mkdir -p $(LIBM_COMMON_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -M -MG $<) > $@

$(LIBM_COMMON_OBJ_DIR)/%.d: $(LIBM_COMMON_DIR)/%.S
	@if [ ! -d $(LIBM_COMMON_OBJ_DIR) ]; then mkdir -p $(LIBM_COMMON_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -M -MG $<) > $@

$(LIBM_COMMON_OBJ_DIR)/%.o: $(LIBM_COMMON_DIR)/%.S
	@if [ ! -d $(LIBM_COMMON_OBJ_DIR) ]; then mkdir -p $(LIBM_COMMON_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -o $@

