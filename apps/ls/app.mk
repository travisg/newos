LSAPP_DIR = $(APPS_DIR)/ls
LSAPP_OBJ_DIR = $(LSAPP_DIR)/$(OBJ_DIR)
LSAPP_OBJS = \
	$(LSAPP_OBJ_DIR)/main.o

DEPS += $(LSAPP_OBJS:.o=.d)

LSAPP = $(LSAPP_OBJ_DIR)/ls

$(LSAPP): $(LSAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(LSAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

lsappclean:
	rm -f $(LSAPP_OBJS) $(LSAPP)

APPS += $(LSAPP)

APPS_CLEAN += lsappclean

$(LSAPP_OBJ_DIR)/%.o: $(LSAPP_DIR)/%.c
	@if [ ! -d $(LSAPP_OBJ_DIR) ]; then mkdir -p $(LSAPP_OBJ_DIR); fi
	@mkdir -p $(LSAPP_OBJ_DIR)
	$(CC) -c $< $(APPS_CFLAGS) $(APPS_INCLUDES) -o $@

$(LSAPP_OBJ_DIR)/%.d: $(LSAPP_DIR)/%.c
	@if [ ! -d $(LSAPP_OBJ_DIR) ]; then mkdir -p $(LSAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(APPS_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
