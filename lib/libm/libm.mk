LIBM_DIR = $(LIB_DIR)/libm
LIBM_OBJ_DIR = $(LIBM_DIR)/$(OBJ_DIR)


include $(LIBM_DIR)/common/libm_common.mk
include $(LIBM_DIR)/common_source/libm_common_source.mk
include $(LIBM_DIR)/ieee/libm_ieee.mk
include $(LIBM_DIR)/arch/$(ARCH)/libm_arch.mk


CFLAGS += 

LIBM_OBJS = \
	$(LIBM_COMMON_OBJS) \
	$(LIBM_COMMON_SOURCE_OBJS) \
	$(LIBM_IEEE_OBJS) \
	$(LIBM_ARCH_OBJS) \

DEPS += $(LIBM_OBJS:.o=.d)

LIBM= $(LIBM_STATIC) $(LIBM_DYNAMIC)
LIBM_STATIC  = $(LIBM_OBJ_DIR)/libm.a
LIBM_DYNAMIC = $(LIBM_OBJ_DIR)/libm.so

$(LIBM_STATIC): $(LIBM_OBJS)
	@mkdir -p $(LIBM_OBJ_DIR)
	$(AR) r $@ $^

$(LIBM_DYNAMIC): $(LIBGLUE) $(LIBM_OBJS)
	@mkdir -p $(LIBM_OBJ_DIR)
	$(LD) -shared -soname libm.so --script=$(LIBS_LDSCRIPT) -o$@ $^

LIBS += $(LIBM_DYNAMIC) 
LINK_LIBS += -Bdynamic $(LIBM_DYNAMIC)
KLIBS += $(LIBM_STATIC)
LINK_KLIBS += -static $(LIBM_STATIC)

nulibmclean:
	rm -f $(LIBM_OBJS) $(LIBM_STATIC) $(LIBM_DYNAMIC)

LIBS_CLEAN += nulibmclean 

# build prototypes
$(LIBM_OBJ_DIR)/%.o: $(LIBM_DIR)/%.c 
	@if [ ! -d $(LIBM_OBJ_DIR) ]; then mkdir -p $(LIBM_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

$(LIBM_OBJ_DIR)/%.d: $(LIBM_DIR)/%.c
	@if [ ! -d $(LIBM_OBJ_DIR) ]; then mkdir -p $(LIBM_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(LIBM_OBJ_DIR)/%.d: $(LIBM_DIR)/%.S
	@if [ ! -d $(LIBM_OBJ_DIR) ]; then mkdir -p $(LIBM_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -M -MG $<) > $@

$(LIBM_OBJ_DIR)/%.o: $(LIBM_DIR)/%.S
	@if [ ! -d $(LIBM_OBJ_DIR) ]; then mkdir -p $(LIBM_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -Iinclude/nulibc -o $@

