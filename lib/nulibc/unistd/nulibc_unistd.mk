NULIBC_UNISTD_DIR = $(NULIBC_DIR)/unistd
NULIBC_UNISTD_OBJ_DIR = $(NULIBC_UNISTD_DIR)/$(OBJ_DIR)

NULIBC_UNISTD_OBJS = \
	$(NULIBC_UNISTD_OBJ_DIR)/close.o \
	$(NULIBC_UNISTD_OBJ_DIR)/dup.o \
	$(NULIBC_UNISTD_OBJ_DIR)/dup2.o \
	$(NULIBC_UNISTD_OBJ_DIR)/lseek.o \
	$(NULIBC_UNISTD_OBJ_DIR)/open.o \
	$(NULIBC_UNISTD_OBJ_DIR)/pread.o \
	$(NULIBC_UNISTD_OBJ_DIR)/pwrite.o \
	$(NULIBC_UNISTD_OBJ_DIR)/read.o \
	$(NULIBC_UNISTD_OBJ_DIR)/write.o

DEPS += $(NULIBC_UNISTD_OBJS:.o=.d)


# build prototypes
$(NULIBC_UNISTD_OBJ_DIR)/%.o: $(NULIBC_UNISTD_DIR)/%.c 
	@if [ ! -d $(NULIBC_UNISTD_OBJ_DIR) ]; then mkdir -p $(NULIBC_UNISTD_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_UNISTD_OBJ_DIR)/%.d: $(NULIBC_UNISTD_DIR)/%.c
	@if [ ! -d $(NULIBC_UNISTD_OBJ_DIR) ]; then mkdir -p $(NULIBC_UNISTD_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_UNISTD_OBJ_DIR)/%.d: $(NULIBC_UNISTD_DIR)/%.S
	@if [ ! -d $(NULIBC_UNISTD_OBJ_DIR) ]; then mkdir -p $(NULIBC_UNISTD_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_UNISTD_OBJ_DIR)/%.o: $(NULIBC_UNISTD_DIR)/%.S
	@if [ ! -d $(NULIBC_UNISTD_OBJ_DIR) ]; then mkdir -p $(NULIBC_UNISTD_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

