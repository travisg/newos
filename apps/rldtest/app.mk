RLDTESTAPP_DIR = $(APPS_DIR)/rldtest
RLDTESTAPP_OBJ_DIR = $(RLDTESTAPP_DIR)/$(OBJ_DIR)
RLDTESTAPP_OBJS = \
	$(RLDTESTAPP_OBJ_DIR)/rldtest.o
RLDTESTLIB_OBJS = \
	$(RLDTESTAPP_OBJ_DIR)/shared.o
RLDGIRLFRIEND_OBJS = \
	$(RLDTESTAPP_OBJ_DIR)/girlfriend.o

DEPS += $(RLDTESTAPP_OBJS:.o=.d) \
	$(RLDTESTLIB_OBJS:.o=.d) \
	$(RLDGIRLFRIEND_OBJS:.o=.d)

RLDTESTAPP = $(RLDTESTAPP_OBJ_DIR)/rldtest
RLDTESTLIB = $(RLDTESTAPP_OBJ_DIR)/librldtest.so
RLDGIRLFRIEND = $(RLDTESTAPP_OBJ_DIR)/girlfriend.so

$(RLDTESTAPP): $(RLDTESTAPP_OBJS) $(LIBS) $(GLUE) $(RLDTESTLIB)
	$(LD) -shared -dynamic -d --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -L$(RLDTESTAPP_OBJ_DIR) -o $@ $(GLUE) $(RLDTESTAPP_OBJS) -lrldtest $(LINK_LIBS) $(LIBGCC)
$(RLDTESTLIB): $(RLDTESTLIB_OBJS) $(LIBS) $(GLUE)
	$(LD) -shared -dynamic -d --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(LIBGLUE) $(RLDTESTLIB_OBJS) 
$(RLDGIRLFRIEND): $(RLDGIRLFRIEND_OBJS) $(LIBS) $(GLUE)
	$(LD) -shared -dynamic -d --script=$(APPS_LDSCRIPT) -L $(LIBGCC_PATH) -o $@ $(LIBGLUE) $(RLDGIRLFRIEND_OBJS) 

rldtestappclean:
	rm -f $(RLDTESTAPP_OBJS) $(RLDTESTAPP) $(RLDTESTLIB)

APPS += $(RLDTESTAPP) $(RLDTESTLIB) $(RLDGIRLFRIEND)

APPS_CLEAN += rldtestappclean

$(RLDTESTAPP_OBJ_DIR)/%.o: $(RLDTESTAPP_DIR)/%.c
	@if [ ! -d $(RLDTESTAPP_OBJ_DIR) ]; then mkdir -p $(RLDTESTAPP_OBJ_DIR); fi
	@mkdir -p $(RLDTESTAPP_OBJ_DIR)
	$(CC) -c $< $(APPS_CFLAGS) -Wunused $(APPS_INCLUDES) -o $@

$(RLDTESTAPP_OBJ_DIR)/%.d: $(RLDTESTAPP_DIR)/%.c
	@if [ ! -d $(RLDTESTAPP_OBJ_DIR) ]; then mkdir -p $(RLDTESTAPP_OBJ_DIR); fi
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@); $(CC) $(APPS_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@

$(RLDTESTAPP_OBJ_DIR)/%.d: $(RLDTESTAPP_DIR)/%.S
	@mkdir -p $(RLDTESTAPP_OBJ_DIR)
	@echo "making deps for $<..."
	@($(ECHO) -n $(dir $@);$(CC) $(APPS_CFLAGS) $(APPS_INCLUDES) -M -MG $<) > $@

$(RLDTESTAPP_OBJ_DIR)/%.o: $(RLDTESTAPP_DIR)/%.S
	@mkdir -p $(RLDTESTAPP_OBJ_DIR)
	$(CC) -c $< $(APPS_CFLAGS) $(APPS_INCLUDES) -o $@
