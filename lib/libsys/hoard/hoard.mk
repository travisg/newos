LIBHOARD_DIR = $(LIBSYS_DIR)/hoard
LIBHOARD_OBJ_DIR = $(LIBHOARD_DIR)/$(OBJ_DIR)
LIBHOARD_OBJS += \
	$(LIBHOARD_OBJ_DIR)/arch-specific.o \
	$(LIBHOARD_OBJ_DIR)/processheap.o \
	$(LIBHOARD_OBJ_DIR)/heap.o \
	$(LIBHOARD_OBJ_DIR)/superblock.o \
	$(LIBHOARD_OBJ_DIR)/threadheap.o \
	$(LIBHOARD_OBJ_DIR)/wrapper.o

DEPS += $(LIBHOARD_OBJS:.o=.d)

LIBHOARD = $(LIBHOARD_OBJ_DIR)/libhoard.o

$(LIBHOARD): $(LIBHOARD_OBJS)
	@mkdir -p $(LIBHOARD_OBJ_DIR)
	$(LD) $(GLOBAL_LDFLAGS) -r -o $@ $(LIBHOARD_OBJS)

LIBHOARD_CFLAGS += -DUSE_PRIVATE_HEAPS=0 -DUSE_SPROC=0 -DNODEBUG -D_REENTRANT=1 -DPACKAGE=\"libhoard\" -DVERSION=\"2.0\"

libhoardclean:
	rm -f $(LIBHOARD_OBJS) $(LIBHOARD)

# build prototypes
$(LIBHOARD_OBJ_DIR)/%.o: $(LIBHOARD_DIR)/%.cpp 
	@if [ ! -d $(LIBHOARD_OBJ_DIR) ]; then mkdir -p $(LIBHOARD_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(LIBHOARD_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(LIBHOARD_OBJ_DIR)/%.d: $(LIBHOARD_DIR)/%.cpp
	@if [ ! -d $(LIBHOARD_OBJ_DIR) ]; then mkdir -p $(LIBHOARD_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(LIBHOARD_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(LIBHOARD_OBJ_DIR)/%.o: $(LIBHOARD_DIR)/%.c 
	@if [ ! -d $(LIBHOARD_OBJ_DIR) ]; then mkdir -p $(LIBHOARD_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) $(LIBHOARD_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(LIBHOARD_OBJ_DIR)/%.d: $(LIBHOARD_DIR)/%.c
	@if [ ! -d $(LIBHOARD_OBJ_DIR) ]; then mkdir -p $(LIBHOARD_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(LIBHOARD_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@
