LIBM_COMMON_SOURCE_DIR = $(LIBM_DIR)/common_source
LIBM_COMMON_SOURCE_OBJ_DIR = $(LIBM_COMMON_SOURCE_DIR)/$(OBJ_DIR)

LIBM_COMMON_SOURCE_OBJS = \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/acosh.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/asincos.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/asinh.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/atan.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/atanh.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/cosh.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/erf.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/exp.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/exp__E.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/expm1.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/floor.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/fmod.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/gamma.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/j0.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/j1.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/jn.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/lgamma.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/log.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/log10.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/log1p.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/log__L.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/pow.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/sinh.o \
	$(LIBM_COMMON_SOURCE_OBJ_DIR)/tanh.o

DEPS += $(LIBM_COMMON_SOURCE_OBJS:.o=.d)


# build prototypes
$(LIBM_COMMON_SOURCE_OBJ_DIR)/%.o: $(LIBM_COMMON_SOURCE_DIR)/%.c 
	@if [ ! -d $(LIBM_COMMON_SOURCE_OBJ_DIR) ]; then mkdir -p $(LIBM_COMMON_SOURCE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -o $@

$(LIBM_COMMON_SOURCE_OBJ_DIR)/%.d: $(LIBM_COMMON_SOURCE_DIR)/%.c
	@if [ ! -d $(LIBM_COMMON_SOURCE_OBJ_DIR) ]; then mkdir -p $(LIBM_COMMON_SOURCE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -M -MG $<) > $@

$(LIBM_COMMON_SOURCE_OBJ_DIR)/%.d: $(LIBM_COMMON_SOURCE_DIR)/%.S
	@if [ ! -d $(LIBM_COMMON_SOURCE_OBJ_DIR) ]; then mkdir -p $(LIBM_COMMON_SOURCE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -M -MG $<) > $@

$(LIBM_COMMON_SOURCE_OBJ_DIR)/%.o: $(LIBM_COMMON_SOURCE_DIR)/%.S
	@if [ ! -d $(LIBM_COMMON_SOURCE_OBJ_DIR) ]; then mkdir -p $(LIBM_COMMON_SOURCE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -I$(LIBM_DIR)/common_source -o $@

