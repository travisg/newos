MOUNTAPP_DIR = $(APPS_DIR)/mount
MOUNTAPP_OBJ_DIR = $(MOUNTAPP_DIR)/$(OBJ_DIR)
MOUNTAPP_OBJS = \
	$(MOUNTAPP_OBJ_DIR)/main.o

DEPS += $(MOUNTAPP_OBJS:.o=.d)

MOUNTAPP = $(MOUNTAPP_OBJ_DIR)/mount

$(MOUNTAPP): $(MOUNTAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(MOUNTAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

mountappclean:
	rm -f $(MOUNTAPP_OBJS) $(MOUNTAPP)

APPS += $(MOUNTAPP)

APPS_CLEAN += mountappclean

$(MOUNTAPP_OBJ_DIR)/%.o: $(MOUNTAPP_DIR)/%.c
	@if [ ! -d $(MOUNTAPP_OBJ_DIR) ]; then mkdir -p $(MOUNTAPP_OBJ_DIR); fi
	@mkdir -p $(MOUNTAPP_OBJ_DIR)
	$(CC) -c $< $(APPS_CFLAGS) $(APPS_INCLUDES) -o $@

$(MOUNTAPP_OBJ_DIR)/%.d: $(MOUNTAPP_DIR)/%.c
	@if [ ! -d $(MOUNTAPP_OBJ_DIR) ]; then mkdir -p $(MOUNTAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(APPS_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
