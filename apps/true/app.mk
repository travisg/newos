TRUEAPP_DIR = $(APPS_DIR)/true
TRUEAPP_OBJ_DIR = $(TRUEAPP_DIR)/$(OBJ_DIR)
TRUEAPP_OBJS = \
	$(TRUEAPP_OBJ_DIR)/main.o

DEPS += $(TRUEAPP_OBJS:.o=.d)

TRUEAPP = $(TRUEAPP_OBJ_DIR)/true

$(TRUEAPP): $(TRUEAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) -dN --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(TRUEAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

trueappclean:
	rm -f $(TRUEAPP_OBJS) $(TRUEAPP)

APPS += $(TRUEAPP)

APPS_CLEAN += trueappclean

$(TRUEAPP_OBJ_DIR)/%.o: $(TRUEAPP_DIR)/%.c
	@if [ ! -d $(TRUEAPP_OBJ_DIR) ]; then mkdir -p $(TRUEAPP_OBJ_DIR); fi
	@mkdir -p $(TRUEAPP_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -o $@

$(TRUEAPP_OBJ_DIR)/%.d: $(TRUEAPP_DIR)/%.c
	@if [ ! -d $(TRUEAPP_OBJ_DIR) ]; then mkdir -p $(TRUEAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
