VMTESTAPP_DIR = $(APPS_DIR)/vmtest
VMTESTAPP_OBJ_DIR = $(VMTESTAPP_DIR)/$(OBJ_DIR)
VMTESTAPP_OBJS = \
	$(VMTESTAPP_OBJ_DIR)/main.o

DEPS += $(VMTESTAPP_OBJS:.o=.d)

VMTESTAPP = $(VMTESTAPP_OBJ_DIR)/vmtest

$(VMTESTAPP): $(VMTESTAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) -dN --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(VMTESTAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

vmtestappclean:
	rm -f $(VMTESTAPP_OBJS) $(VMTESTAPP)

APPS += $(VMTESTAPP)

APPS_CLEAN += vmtestappclean

$(VMTESTAPP_OBJ_DIR)/%.o: $(VMTESTAPP_DIR)/%.c
	@mkdir -p $(VMTESTAPP_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) -O0 $(APPS_INCLUDES) -o $@

$(VMTESTAPP_OBJ_DIR)/%.d: $(VMTESTAPP_DIR)/%.c
	@mkdir -p $(VMTESTAPP_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
