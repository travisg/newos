TESTAPP_DIR = $(APPS_DIR)/testapp
TESTAPP_OBJ_DIR = $(TESTAPP_DIR)/$(OBJ_DIR)
TESTAPP_OBJS = \
	$(TESTAPP_OBJ_DIR)/main.o

DEPS += $(TESTAPP_OBJS:.o=.d)

TESTAPP = $(TESTAPP_OBJ_DIR)/testapp

$(TESTAPP): $(TESTAPP_OBJS) $(LIBS)
	$(LD) -dN --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(TESTAPP_OBJS) $(LIBS) $(LIBGCC)

testappclean:
	rm -f $(TESTAPP_OBJS) $(TESTAPP)

APPS += $(TESTAPP)

APPS_CLEAN += testappclean

$(TESTAPP_OBJ_DIR)/%.o: $(TESTAPP_DIR)/%.c
	@mkdir -p $(TESTAPP_OBJ_DIR)
	$(CC) -c $< $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -o $@

$(TESTAPP_OBJ_DIR)/%.d: $(TESTAPP_DIR)/%.c
	@mkdir -p $(TESTAPP_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(GLOBAL_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@
