NULIBC_LOCALE_DIR = $(NULIBC_DIR)/locale
NULIBC_LOCALE_OBJ_DIR = $(NULIBC_LOCALE_DIR)/$(OBJ_DIR)

NULIBC_OBJS= $(NULIBC_LOCALE_OBJS)
NULIBC_LOCALE_OBJS = \
	$(NULIBC_LOCALE_OBJ_DIR)/ctype.o

DEPS += $(NULIBC_LOCALE_OBJS:.o=.d)


# build prototypes
$(NULIBC_LOCALE_OBJ_DIR)/%.o: $(NULIBC_LOCALE_DIR)/%.c 
	@if [ ! -d $(NULIBC_LOCALE_OBJ_DIR) ]; then mkdir -p $(NULIBC_LOCALE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_LOCALE_OBJ_DIR)/%.d: $(NULIBC_LOCALE_DIR)/%.c
	@if [ ! -d $(NULIBC_LOCALE_OBJ_DIR) ]; then mkdir -p $(NULIBC_LOCALE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_LOCALE_OBJ_DIR)/%.d: $(NULIBC_LOCALE_DIR)/%.S
	@if [ ! -d $(NULIBC_LOCALE_OBJ_DIR) ]; then mkdir -p $(NULIBC_LOCALE_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_LOCALE_OBJ_DIR)/%.o: $(NULIBC_LOCALE_DIR)/%.S
	@if [ ! -d $(NULIBC_LOCALE_OBJ_DIR) ]; then mkdir -p $(NULIBC_LOCALE_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

