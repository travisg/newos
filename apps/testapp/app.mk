TESTAPP_DIR = $(APPS_DIR)/testapp
TESTAPP_OBJ_DIR = $(TESTAPP_DIR)/$(OBJ_DIR)
TESTAPP_OBJS = \
	$(TESTAPP_OBJ_DIR)/main.o

DEPS += $(TESTAPP_OBJS:.o=.d)

TESTAPP = $(TESTAPP_OBJ_DIR)/testapp

$(TESTAPP): $(TESTAPP_OBJS) $(LIBS) $(GLUE)
	$(LD) --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(GLUE) $(TESTAPP_OBJS) $(LINK_LIBS) $(LIBGCC)

testappclean:
	rm -f $(TESTAPP_OBJS) $(TESTAPP)

APPS += $(TESTAPP)

APPS_CLEAN += testappclean

$(TESTAPP_OBJ_DIR)/%.o: $(TESTAPP_DIR)/%.c
	@if [ ! -d $(TESTAPP_OBJ_DIR) ]; then mkdir -p $(TESTAPP_OBJ_DIR); fi
	@mkdir -p $(TESTAPP_OBJ_DIR)
	$(CC) -c $< $(APPS_CFLAGS) $(APPS_INCLUDES) -o $@

$(TESTAPP_OBJ_DIR)/%.d: $(TESTAPP_DIR)/%.c
	@if [ ! -d $(TESTAPP_OBJ_DIR) ]; then mkdir -p $(TESTAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(APPS_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
