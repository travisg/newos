INITAPP_DIR = $(APPS_DIR)/init
INITAPP_OBJ_DIR = $(INITAPP_DIR)/$(OBJ_DIR)
INITAPP_OBJS = \
	$(INITAPP_OBJ_DIR)/init.o

DEPS += $(INITAPP_OBJS:.o=.d)

INITAPP = $(INITAPP_OBJ_DIR)/init

$(INITAPP): $(INITAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) -dN --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(INITAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

initappclean:
	rm -f $(INITAPP_OBJS) $(INITAPP)

APPS += $(INITAPP)

APPS_CLEAN += initappclean

$(INITAPP_OBJ_DIR)/%.o: $(INITAPP_DIR)/%.c
	@if [ ! -d $(INITAPP_OBJ_DIR) ]; then mkdir -p $(INITAPP_OBJ_DIR); fi
	$(CC) -c $< $(APPS_CFLAGS) $(APPS_INCLUDES) -o $@

$(INITAPP_OBJ_DIR)/%.d: $(INITAPP_DIR)/%.c
	@if [ ! -d $(INITAPP_OBJ_DIR) ]; then mkdir -p $(INITAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(APPS_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
