UNMOUNTAPP_DIR = $(APPS_DIR)/unmount
UNMOUNTAPP_OBJ_DIR = $(UNMOUNTAPP_DIR)/$(OBJ_DIR)
UNMOUNTAPP_OBJS = \
	$(UNMOUNTAPP_OBJ_DIR)/main.o

DEPS += $(UNMOUNTAPP_OBJS:.o=.d)

UNMOUNTAPP = $(UNMOUNTAPP_OBJ_DIR)/unmount

$(UNMOUNTAPP): $(UNMOUNTAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(UNMOUNTAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

unmountappclean:
	rm -f $(UNMOUNTAPP_OBJS) $(UNMOUNTAPP)

APPS += $(UNMOUNTAPP)

APPS_CLEAN += unmountappclean

$(UNMOUNTAPP_OBJ_DIR)/%.o: $(UNMOUNTAPP_DIR)/%.c
	@if [ ! -d $(UNMOUNTAPP_OBJ_DIR) ]; then mkdir -p $(UNMOUNTAPP_OBJ_DIR); fi
	@mkdir -p $(UNMOUNTAPP_OBJ_DIR)
	$(CC) -c $< $(APPS_CFLAGS) $(APPS_INCLUDES) -o $@

$(UNMOUNTAPP_OBJ_DIR)/%.d: $(UNMOUNTAPP_DIR)/%.c
	@if [ ! -d $(UNMOUNTAPP_OBJ_DIR) ]; then mkdir -p $(UNMOUNTAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(APPS_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
