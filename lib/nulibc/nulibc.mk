NULIBC_DIR = $(LIB_DIR)/nulibc
NULIBC_OBJ_DIR = $(NULIBC_DIR)/$(OBJ_DIR)


#include $(NULIBC_DIR)/hoard/nulibc_hoard.mk
include $(NULIBC_DIR)/locale/nulibc_locale.mk
include $(NULIBC_DIR)/stdio/nulibc_stdio.mk
include $(NULIBC_DIR)/stdlib/nulibc_stdlib.mk
include $(NULIBC_DIR)/string/nulibc_string.mk
include $(NULIBC_DIR)/system/nulibc_system.mk

CFLAGS += -I$(INCL

NULIBC_OBJS = \
	$(NULIBC_HOARD_OBJS) \
	$(NULIBC_LOCALE_OBJS) \
	$(NULIBC_STDIO_OBJS) \
	$(NULIBC_STDLIB_OBJS) \
	$(NULIBC_STRING_OBJS) \
	$(NULIBC_SYSTEM_OBJS)

DEPS += $(NULIBC_OBJS:.o=.d)

NULIBC= $(NULIBC_STATIC) $(NULIBC_DYNAMIC)
NULIBC_STATIC  = $(NULIBC_OBJ_DIR)/libc.a
NULIBC_DYNAMIC = $(NULIBC_OBJ_DIR)/libc.so

$(NULIBC_STATIC): $(NULIBC_OBJS)
	@mkdir -p $(NULIBC_OBJ_DIR)
	$(AR) r $@ $^

$(NULIBC_DYNAMIC): $(NULIBC_OBJS)
	@mkdir -p $(NULIBC_OBJ_DIR)
	$(LD) -shared -soname libc.so --script=$(NULIBC_DIR)/library.ld -o$@ $^

LIBS += $(NULIBC_DYNAMIC) 
LINK_LIBS += -Bdynamic $(NULIBC_DYNAMIC)
KLIBS += $(NULIBC_STATIC)
LINK_KLIBS += -static $(NULIBC_STATIC)

nulibcclean:
	rm -f $(NULIBC_OBJS) $(NULIBC_STATIC) $(NULIBC_DYNAMIC)

LIBS_CLEAN += nulibcclean 

# build prototypes
$(NULIBC_OBJ_DIR)/%.o: $(NULIBC_DIR)/%.c 
	@if [ ! -d $(NULIBC_OBJ_DIR) ]; then mkdir -p $(NULIBC_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(NULIBC_OBJ_DIR)/%.d: $(NULIBC_DIR)/%.c
	@if [ ! -d $(NULIBC_OBJ_DIR) ]; then mkdir -p $(NULIBC_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_OBJ_DIR)/%.d: $(NULIBC_DIR)/%.S
	@if [ ! -d $(NULIBC_OBJ_DIR) ]; then mkdir -p $(NULIBC_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(NULIBC_OBJ_DIR)/%.o: $(NULIBC_DIR)/%.S
	@if [ ! -d $(NULIBC_OBJ_DIR) ]; then mkdir -p $(NULIBC_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

