LIBC_DIR = $(LIB_DIR)/libc
LIBC_OBJS = \
	$(LIBC_DIR)/atoi.o \
	$(LIBC_DIR)/bcopy.o \
	$(LIBC_DIR)/ctype.o \
	$(LIBC_DIR)/memcmp.o \
	$(LIBC_DIR)/memcpy.o \
	$(LIBC_DIR)/memmove.o \
	$(LIBC_DIR)/memscan.o \
	$(LIBC_DIR)/memset.o \
	$(LIBC_DIR)/strcat.o \
	$(LIBC_DIR)/strchr.o \
	$(LIBC_DIR)/strcmp.o \
	$(LIBC_DIR)/strcpy.o \
	$(LIBC_DIR)/strlen.o \
	$(LIBC_DIR)/strncat.o \
	$(LIBC_DIR)/strncmp.o \
	$(LIBC_DIR)/strncpy.o \
	$(LIBC_DIR)/strnicmp.o \
	$(LIBC_DIR)/strnlen.o \
	$(LIBC_DIR)/strpbrk.o \
	$(LIBC_DIR)/strrchr.o \
	$(LIBC_DIR)/strspn.o \
	$(LIBC_DIR)/strstr.o \
	$(LIBC_DIR)/strtok.o \
	$(LIBC_DIR)/vsprintf.o

LIBC_DEPS = $(LIBC_OBJS:.o=.d)

LIBC = $(LIBC_DIR)/libc.a

$(LIBC): $(LIBC_OBJS)
	$(AR) r $@ $(LIBC_OBJS)

LIBS += $(LIBC) 

libcclean:
	rm -f $(LIBC_OBJS) $(LIBC)

LIBS_CLEAN += libcclean 

include $(LIBC_DEPS)

# build prototypes
$(LIBC_DIR)/%.o: $(LIBC_DIR)/%.c 
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

$(LIBC_DIR)/%.d: $(LIBC_DIR)/%.c
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBC_DIR)/%.d: $(LIBC_DIR)/%.S
	@echo "making deps for $<..."
	@(echo -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBC_DIR)/%.o: $(LIBC_DIR)/%.S
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

