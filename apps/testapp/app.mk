TESTAPP_DIR = $(APPS_DIR)/testapp

TESTAPP_OBJS = \
	$(TESTAPP_DIR)/main.o

DEPS += $(TESTAPP_OBJS:.o=.d)

TESTAPP = $(TESTAPP_DIR)/testapp

$(TESTAPP): $(TESTAPP_OBJS) $(LIBS)
	$(LD) -dN --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(TESTAPP_OBJS) $(LIBS) $(LIBGCC)

testappclean:
	rm -f $(TESTAPP_OBJS) $(TESTAPP)

APPS += $(TESTAPP)

APPS_CLEAN += testappclean
