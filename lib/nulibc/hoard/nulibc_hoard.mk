NULIBC_HOARD_DIR = $(NULIBC_DIR)/hoard
NULIBC_HOARD_OBJ_DIR = $(NULIBC_HOARD_DIR)/$(OBJ_DIR)
NULIBC_HOARD_OBJS += \
	$(NULIBC_HOARD_OBJ_DIR)/arch-specific.o \
	$(NULIBC_HOARD_OBJ_DIR)/processheap.o \
	$(NULIBC_HOARD_OBJ_DIR)/heap.o \
	$(NULIBC_HOARD_OBJ_DIR)/superblock.o \
	$(NULIBC_HOARD_OBJ_DIR)/threadheap.o \
	$(NULIBC_HOARD_OBJ_DIR)/wrapper.o

DEPS += $(NULIBC_HOARD_OBJS:.o=.d)

NULIBC_HOARD_CFLAGS += -DUSE_PRIVATE_HEAPS=0 -DUSE_SPROC=0 -DNODEBUG -D_REENTRANT=1 -DPACKAGE=\"libhoard\" -DVERSION=\"2.0\"

# build prototypes
$(NULIBC_HOARD_OBJ_DIR)/%.o: $(NULIBC_HOARD_DIR)/%.cpp 
	@if [ ! -d $(NULIBC_HOARD_OBJ_DIR) ]; then mkdir -p $(NULIBC_HOARD_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(NULIBC_HOARD_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_HOARD_OBJ_DIR)/%.d: $(NULIBC_HOARD_DIR)/%.cpp
	@if [ ! -d $(NULIBC_HOARD_OBJ_DIR) ]; then mkdir -p $(NULIBC_HOARD_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(NULIBC_HOARD_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_HOARD_OBJ_DIR)/%.o: $(NULIBC_HOARD_DIR)/%.c 
	@if [ ! -d $(NULIBC_HOARD_OBJ_DIR) ]; then mkdir -p $(NULIBC_HOARD_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(NULIBC_HOARD_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_HOARD_OBJ_DIR)/%.d: $(NULIBC_HOARD_DIR)/%.c
	@if [ ! -d $(NULIBC_HOARD_OBJ_DIR) ]; then mkdir -p $(NULIBC_HOARD_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(NULIBC_HOARD_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@
