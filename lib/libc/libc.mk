LIBC_DIR = $(LIB_DIR)/libc
LIBC_OBJ_DIR = $(LIBC_DIR)/$(OBJ_DIR)
LIBC_OBJS = \
	$(LIBC_OBJ_DIR)/atoi.o \
	$(LIBC_OBJ_DIR)/bcopy.o \
	$(LIBC_OBJ_DIR)/ctype.o \
	$(LIBC_OBJ_DIR)/memcmp.o \
	$(LIBC_OBJ_DIR)/memcpy.o \
	$(LIBC_OBJ_DIR)/memmove.o \
	$(LIBC_OBJ_DIR)/memscan.o \
	$(LIBC_OBJ_DIR)/memset.o \
	$(LIBC_OBJ_DIR)/strcat.o \
	$(LIBC_OBJ_DIR)/strchr.o \
	$(LIBC_OBJ_DIR)/strcmp.o \
	$(LIBC_OBJ_DIR)/strcpy.o \
	$(LIBC_OBJ_DIR)/strlen.o \
	$(LIBC_OBJ_DIR)/strncat.o \
	$(LIBC_OBJ_DIR)/strncmp.o \
	$(LIBC_OBJ_DIR)/strncpy.o \
	$(LIBC_OBJ_DIR)/strnicmp.o \
	$(LIBC_OBJ_DIR)/strnlen.o \
	$(LIBC_OBJ_DIR)/strpbrk.o \
	$(LIBC_OBJ_DIR)/strrchr.o \
	$(LIBC_OBJ_DIR)/strspn.o \
	$(LIBC_OBJ_DIR)/strstr.o \
	$(LIBC_OBJ_DIR)/strtok.o \
	$(LIBC_OBJ_DIR)/vsprintf.o

DEPS += $(LIBC_OBJS:.o=.d)

LIBC = $(LIBC_OBJ_DIR)/libc.a

$(LIBC): $(LIBC_OBJS)
	$(AR) r $@ $(LIBC_OBJS)

LIBS += $(LIBC) 
LINK_LIBS += $(LIBC)
KLIBS += $(LIBC)
LINK_KLIBS += $(LIBC)

libcclean:
	rm -f $(LIBC_OBJS) $(LIBC)

LIBS_CLEAN += libcclean 

# build prototypes
$(LIBC_OBJ_DIR)/%.o: $(LIBC_DIR)/%.c 
	@if [ ! -d $(LIBC_OBJ_DIR) ]; then mkdir -p $(LIBC_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

$(LIBC_OBJ_DIR)/%.d: $(LIBC_DIR)/%.c
	@if [ ! -d $(LIBC_OBJ_DIR) ]; then mkdir -p $(LIBC_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBC_OBJ_DIR)/%.d: $(LIBC_DIR)/%.S
	@if [ ! -d $(LIBC_OBJ_DIR) ]; then mkdir -p $(LIBC_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBC_OBJ_DIR)/%.o: $(LIBC_DIR)/%.S
	@if [ ! -d $(LIBC_OBJ_DIR) ]; then mkdir -p $(LIBC_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

