NULIBC_STDIO_DIR = $(NULIBC_DIR)/stdio
NULIBC_STDIO_OBJ_DIR = $(NULIBC_STDIO_DIR)/$(OBJ_DIR)

NULIBC_OBJS= $(NULIBC_STDIO_OBJS)
NULIBC_STDIO_OBJS = \
	$(NULIBC_STDIO_OBJ_DIR)/vsprintf.o \
	$(NULIBC_STDIO_OBJ_DIR)/stdio.o

DEPS += $(NULIBC_STDIO_OBJS:.o=.d)


# build prototypes
$(NULIBC_STDIO_OBJ_DIR)/%.o: $(NULIBC_STDIO_DIR)/%.c 
	@if [ ! -d $(NULIBC_STDIO_OBJ_DIR) ]; then mkdir -p $(NULIBC_STDIO_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_STDIO_OBJ_DIR)/%.d: $(NULIBC_STDIO_DIR)/%.c
	@if [ ! -d $(NULIBC_STDIO_OBJ_DIR) ]; then mkdir -p $(NULIBC_STDIO_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_STDIO_OBJ_DIR)/%.d: $(NULIBC_STDIO_DIR)/%.S
	@if [ ! -d $(NULIBC_STDIO_OBJ_DIR) ]; then mkdir -p $(NULIBC_STDIO_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_STDIO_OBJ_DIR)/%.o: $(NULIBC_STDIO_DIR)/%.S
	@if [ ! -d $(NULIBC_STDIO_OBJ_DIR) ]; then mkdir -p $(NULIBC_STDIO_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

