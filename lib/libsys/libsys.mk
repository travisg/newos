LIBSYS_DIR = $(LIB_DIR)/libsys
LIBSYS_OBJ_DIR = $(LIBSYS_DIR)/$(OBJ_DIR)
LIBSYS_OBJS = \
	$(LIBSYS_OBJ_DIR)/stdio.o

DEPS += $(LIBSYS_OBJS:.o=.d)

LIBSYS = $(LIBSYS_OBJ_DIR)/libsys.o

LIBSYS_ARCH_DIR = $(LIBSYS_DIR)/arch/$(ARCH)
include $(LIBSYS_ARCH_DIR)/arch_libsys.mk

$(LIBSYS): $(LIBSYS_OBJS)
	@mkdir -p $(LIBSYS_OBJ_DIR)
	$(LD) $(GLOBAL_LDFLAGS) -r -o $@ $(LIBSYS_OBJS)

LIBS += $(LIBSYS) 
LINK_LIBS += $(LIBSYS) 

libsysclean:
	rm -f $(LIBSYS_OBJS) $(LIBSYS)

LIBS_CLEAN += libsysclean 

# build prototypes
$(LIBSYS_OBJ_DIR)/%.o: $(LIBSYS_DIR)/%.c 
	@if [ ! -d $(LIBSYS_OBJ_DIR) ]; then mkdir -p $(LIBSYS_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

$(LIBSYS_OBJ_DIR)/%.d: $(LIBSYS_DIR)/%.c
	@if [ ! -d $(LIBSYS_OBJ_DIR) ]; then mkdir -p $(LIBSYS_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBSYS_OBJ_DIR)/%.d: $(LIBSYS_DIR)/%.S
	@if [ ! -d $(LIBSYS_OBJ_DIR) ]; then mkdir -p $(LIBSYS_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) -Iinclude -M -MG $<) > $@

$(LIBSYS_OBJ_DIR)/%.o: $(LIBSYS_DIR)/%.S
	@if [ ! -d $(LIBSYS_OBJ_DIR) ]; then mkdir -p $(LIBSYS_OBJ_DIR); fi
	$(CC) -c $< $(GLOBAL_CFLAGS) -Iinclude -o $@

