LIB_DIR = lib
LIB_OBJS = \
	$(LIB_DIR)/atoi.o \
	$(LIB_DIR)/bcopy.o \
	$(LIB_DIR)/ctype.o \
	$(LIB_DIR)/memcmp.o \
	$(LIB_DIR)/memcpy.o \
	$(LIB_DIR)/memmove.o \
	$(LIB_DIR)/memscan.o \
	$(LIB_DIR)/memset.o \
	$(LIB_DIR)/strcat.o \
	$(LIB_DIR)/strchr.o \
	$(LIB_DIR)/strcmp.o \
	$(LIB_DIR)/strcpy.o \
	$(LIB_DIR)/strlen.o \
	$(LIB_DIR)/strncat.o \
	$(LIB_DIR)/strncmp.o \
	$(LIB_DIR)/strncpy.o \
	$(LIB_DIR)/strnicmp.o \
	$(LIB_DIR)/strnlen.o \
	$(LIB_DIR)/strpbrk.o \
	$(LIB_DIR)/strrchr.o \
	$(LIB_DIR)/strspn.o \
	$(LIB_DIR)/strstr.o \
	$(LIB_DIR)/strtok.o \
	$(LIB_DIR)/vsprintf.o

LIB_DEPS = $(LIB_OBJS:.o=.d)

LIB = $(LIB_DIR)/lib.a

$(LIB): $(LIB_OBJS)
	$(AR) r $@ $(LIB_OBJS)

libclean:
	rm -f $(LIB_OBJS) $(LIB)

include $(LIB_DEPS)

# build prototypes
$(LIB_DIR)/%.o: $(LIB_DIR)/%.c 
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

$(LIB_DIR)/%.d: $(LIB_DIR)/%.c
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIB_DIR)/%.d: $(LIB_DIR)/%.S
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIB_DIR)/%.o: $(LIB_DIR)/%.S
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

