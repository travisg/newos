RLDAPP_DIR = $(APPS_DIR)/rld
RLDAPP_OBJ_DIR = $(RLDAPP_DIR)/$(OBJ_DIR)
RLDAPP_OBJS = \
	$(RLDAPP_OBJ_DIR)/rld0.o \
	$(RLDAPP_OBJ_DIR)/rld.o \
	$(RLDAPP_OBJ_DIR)/rldelf.o \
	$(RLDAPP_OBJ_DIR)/rldheap.o \
	$(RLDAPP_OBJ_DIR)/rldaux.o

DEPS += $(RLDAPP_OBJS:.o=.d)

RLDAPP = $(RLDAPP_OBJ_DIR)/rld.so

$(RLDAPP): $(RLDAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) --script=$(RLDAPP_DIR)/arch/$(ARCH)/rld.ld -L $(LIBGCC_PATH) -o $@ $(RLDAPP_OBJS) $(LIBS_SYSCALLS) $(LIBC) $(LIBGCC)

rldappclean:
	rm -f $(RLDAPP_OBJS) $(RLDAPP)

APPS += $(RLDAPP)

APPS_CLEAN += rldappclean

$(RLDAPP_OBJ_DIR)/%.o: $(RLDAPP_DIR)/%.c
	@if [ ! -d $(RLDAPP_OBJ_DIR) ]; then mkdir -p $(RLDAPP_OBJ_DIR); fi
	@mkdir -p $(RLDAPP_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) -Wunused $(APPS_INCLUDES) -o $@

$(RLDAPP_OBJ_DIR)/%.d: $(RLDAPP_DIR)/%.c
	@if [ ! -d $(RLDAPP_OBJ_DIR) ]; then mkdir -p $(RLDAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@

$(RLDAPP_OBJ_DIR)/%.d: $(RLDAPP_DIR)/%.S
	@mkdir -p $(RLDAPP_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@

$(RLDAPP_OBJ_DIR)/%.o: $(RLDAPP_DIR)/%.S
	@mkdir -p $(RLDAPP_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -o $@
