LIBM_IEEE_DIR = $(LIBM_DIR)/ieee
LIBM_IEEE_OBJ_DIR = $(LIBM_IEEE_DIR)/$(OBJ_DIR)

LIBM_IEEE_OBJS = \
	$(LIBM_IEEE_OBJ_DIR)/cabs.o \
	$(LIBM_IEEE_OBJ_DIR)/cbrt.o \
	$(LIBM_IEEE_OBJ_DIR)/support.o

DEPS += $(LIBM_IEEE_OBJS:.o=.d)


# build prototypes
$(LIBM_IEEE_OBJ_DIR)/%.o: $(LIBM_IEEE_DIR)/%.c 
	@if [ ! -d $(LIBM_IEEE_OBJ_DIR) ]; then mkdir -p $(LIBM_IEEE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -o $@

$(LIBM_IEEE_OBJ_DIR)/%.d: $(LIBM_IEEE_DIR)/%.c
	@if [ ! -d $(LIBM_IEEE_OBJ_DIR) ]; then mkdir -p $(LIBM_IEEE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -M -MG $<) > $@

$(LIBM_IEEE_OBJ_DIR)/%.d: $(LIBM_IEEE_DIR)/%.S
	@if [ ! -d $(LIBM_IEEE_OBJ_DIR) ]; then mkdir -p $(LIBM_IEEE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -M -MG $<) > $@

$(LIBM_IEEE_OBJ_DIR)/%.o: $(LIBM_IEEE_DIR)/%.S
	@if [ ! -d $(LIBM_IEEE_OBJ_DIR) ]; then mkdir -p $(LIBM_IEEE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -o $@

