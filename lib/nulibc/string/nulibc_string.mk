NULIBC_STRING_DIR = $(NULIBC_DIR)/string
NULIBC_STRING_OBJ_DIR = $(NULIBC_STRING_DIR)/$(OBJ_DIR)

NULIBC_STRING_OBJS = \
	$(NULIBC_STRING_OBJ_DIR)/bcopy.o \
	$(NULIBC_STRING_OBJ_DIR)/bzero.o \
	$(NULIBC_STRING_OBJ_DIR)/memchr.o \
	$(NULIBC_STRING_OBJ_DIR)/memcmp.o \
	$(NULIBC_STRING_OBJ_DIR)/memcpy.o \
	$(NULIBC_STRING_OBJ_DIR)/memmove.o \
	$(NULIBC_STRING_OBJ_DIR)/memset.o \
	$(NULIBC_STRING_OBJ_DIR)/strcat.o \
	$(NULIBC_STRING_OBJ_DIR)/strchr.o \
	$(NULIBC_STRING_OBJ_DIR)/strcmp.o \
	$(NULIBC_STRING_OBJ_DIR)/strcpy.o \
	$(NULIBC_STRING_OBJ_DIR)/strerror.o \
	$(NULIBC_STRING_OBJ_DIR)/strlcat.o \
	$(NULIBC_STRING_OBJ_DIR)/strlcpy.o \
	$(NULIBC_STRING_OBJ_DIR)/strlen.o \
	$(NULIBC_STRING_OBJ_DIR)/strncat.o \
	$(NULIBC_STRING_OBJ_DIR)/strncmp.o \
	$(NULIBC_STRING_OBJ_DIR)/strncpy.o \
	$(NULIBC_STRING_OBJ_DIR)/strnicmp.o \
	$(NULIBC_STRING_OBJ_DIR)/strnlen.o \
	$(NULIBC_STRING_OBJ_DIR)/strpbrk.o \
	$(NULIBC_STRING_OBJ_DIR)/strrchr.o \
	$(NULIBC_STRING_OBJ_DIR)/strspn.o \
	$(NULIBC_STRING_OBJ_DIR)/strstr.o \
	$(NULIBC_STRING_OBJ_DIR)/strtok.o

DEPS += $(NULIBC_STRING_OBJS:.o=.d)


# build prototypes
$(NULIBC_STRING_OBJ_DIR)/%.o: $(NULIBC_STRING_DIR)/%.c 
	@if [ ! -d $(NULIBC_STRING_OBJ_DIR) ]; then mkdir -p $(NULIBC_STRING_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_STRING_OBJ_DIR)/%.d: $(NULIBC_STRING_DIR)/%.c
	@if [ ! -d $(NULIBC_STRING_OBJ_DIR) ]; then mkdir -p $(NULIBC_STRING_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_STRING_OBJ_DIR)/%.d: $(NULIBC_STRING_DIR)/%.S
	@if [ ! -d $(NULIBC_STRING_OBJ_DIR) ]; then mkdir -p $(NULIBC_STRING_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_STRING_OBJ_DIR)/%.o: $(NULIBC_STRING_DIR)/%.S
	@if [ ! -d $(NULIBC_STRING_OBJ_DIR) ]; then mkdir -p $(NULIBC_STRING_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

