NULIBC_STDLIB_DIR = $(NULIBC_DIR)/stdlib
NULIBC_STDLIB_OBJ_DIR = $(NULIBC_STDLIB_DIR)/$(OBJ_DIR)

NULIBC_STDLIB_OBJS = \
	$(NULIBC_STDLIB_OBJ_DIR)/assert.o \
	$(NULIBC_STDLIB_OBJ_DIR)/atoi.o \
	$(NULIBC_STDLIB_OBJ_DIR)/bsearch.o \
	$(NULIBC_STDLIB_OBJ_DIR)/heapsort.o \
	$(NULIBC_STDLIB_OBJ_DIR)/merge.o \
	$(NULIBC_STDLIB_OBJ_DIR)/qsort.o \
	$(NULIBC_STDLIB_OBJ_DIR)/radixsort.o \
	$(NULIBC_STDLIB_OBJ_DIR)/rand.o \
	$(NULIBC_STDLIB_OBJ_DIR)/random.o

DEPS += $(NULIBC_STDLIB_OBJS:.o=.d)


# build prototypes
$(NULIBC_STDLIB_OBJ_DIR)/%.o: $(NULIBC_STDLIB_DIR)/%.c 
	@if [ ! -d $(NULIBC_STDLIB_OBJ_DIR) ]; then mkdir -p $(NULIBC_STDLIB_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_STDLIB_OBJ_DIR)/%.d: $(NULIBC_STDLIB_DIR)/%.c
	@if [ ! -d $(NULIBC_STDLIB_OBJ_DIR) ]; then mkdir -p $(NULIBC_STDLIB_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_STDLIB_OBJ_DIR)/%.d: $(NULIBC_STDLIB_DIR)/%.S
	@if [ ! -d $(NULIBC_STDLIB_OBJ_DIR) ]; then mkdir -p $(NULIBC_STDLIB_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_STDLIB_OBJ_DIR)/%.o: $(NULIBC_STDLIB_DIR)/%.S
	@if [ ! -d $(NULIBC_STDLIB_OBJ_DIR) ]; then mkdir -p $(NULIBC_STDLIB_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

